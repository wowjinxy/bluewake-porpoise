// One callback-free private CL body. Diagnostic submission is chosen separately
// by COS_SMOKE=remote-body. No actor, network, gameplay or save authority here.
#include "d/dolzel.h" // IWYU pragma: keep
#include "pc/pc_remote_body.h"
#include "pc/pc_remote_body_asset.h"
#include "pc/pc_harness.h"
#include "pc/pc_interp.h"
#include "JSystem/J3DGraphAnimator/J3DModel.h"
#include "JSystem/J3DGraphAnimator/J3DModelData.h"
#include "JSystem/J3DGraphAnimator/J3DMaterialAnm.h"
#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "JSystem/JKernel/JKRDvdFile.h"
#include "JSystem/JKernel/JKRSolidHeap.h"
#include "JSystem/JUtility/JUTNameTab.h"
#include "d/d_com_inf_game.h"
#include "d/d_drawlist.h"
#include "d/d_kankyo.h"
#include "m_Do/m_Do_ext.h"
#include "dolphin/gd/GDBase.h"
#include "dolphin/os/OS.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include "assets/GZLE01/res/Object/Link.h"

namespace {
constexpr u32 kRendererBytes = 4 * 1024 * 1024;
constexpr u32 kScratchBytes = 12 * 1024 * 1024;
constexpr u32 kArchiveStoredMax = 4 * 1024 * 1024;
constexpr u32 kArchiveExpandedMax = 8 * 1024 * 1024;
constexpr u32 kBinaryMax = 1024 * 1024;
constexpr u32 kToonMax = 64 * 1024;
static_assert(CL_JNT_CL_BACK_e + 1 == PC_REMOTE_BODY_JOINTS, "GZLE01 CL42 contract");

JKRSolidHeap* sHeap;
J3DModelData* sData;
J3DModel* sModel;
u8* sBinary;
u32 sBinaryBytes;
PcRemoteBodyPose sPose{};
bool sAttempted, sReady, sHavePose;
u64 sSceneEpoch;
u32 sSubmittedFrame = std::numeric_limits<u32>::max();
u32 sSubmissions;

struct CurrentHeap {
    JKRHeap* previous;
    explicit CurrentHeap(JKRHeap* heap) : previous(heap->becomeCurrentHeap()) {}
    ~CurrentHeap() { previous->becomeCurrentHeap(); }
};

// Preserve CPU J3D/GD submission state, not just draw-buffer pointers. Packets
// retain their own private resources; buffer contents intentionally keep entries.
struct J3DState {
    J3DSys sys = j3dSys;
    Mtx current;
    Vec scale = J3DSys::mCurrentS, parentScale = J3DSys::mParentS;
    J3DTexCoordScaleInfo texScale[8];
    GDLObj* gd = GDGetCurrent();
    GDLObj staticGD = J3DDisplayListObj::sGDLObj;
    s32 interruptFlag = J3DDisplayListObj::sInterruptFlag;
    J3DState() {
        MTXCopy(J3DSys::mCurrentMtx, current);
        std::memcpy(texScale, J3DSys::sTexCoordScaleTable, sizeof(texScale));
    }
    ~J3DState() {
        j3dSys = sys;
        MTXCopy(current, J3DSys::mCurrentMtx);
        J3DSys::mCurrentS = scale; J3DSys::mParentS = parentScale;
        std::memcpy(J3DSys::sTexCoordScaleTable, texScale, sizeof(texScale));
        J3DDisplayListObj::sGDLObj = staticGD;
        J3DDisplayListObj::sInterruptFlag = interruptFlag;
        GDSetCurrent(gd);
    }
};

bool owned(const void* pointer, size_t length = 1) {
    if (!sHeap || !pointer) return false;
    const uintptr_t p = reinterpret_cast<uintptr_t>(pointer);
    const uintptr_t first = reinterpret_cast<uintptr_t>(sHeap->getStartAddr());
    const uintptr_t last = reinterpret_cast<uintptr_t>(sHeap->getEndAddr());
    return p >= first && p <= last && length <= last - p;
}

void forget_pose() {
    if (!sModel) return;
    pc_interp_forget_mtx(&sModel->getBaseTRMtx());
    for (int i = 0; i < PC_REMOTE_BODY_JOINTS; ++i)
        pc_interp_forget_mtx(sModel->getAnmMtx(i));
    for (u16 i = 0; i < sData->getWEvlpMtxNum(); ++i)
        pc_interp_forget_mtx(sModel->getWeightAnmMtx(i));
}

bool finite_pose(const PcRemoteBodyPose& p) {
    if (p.version != PC_REMOTE_BODY_POSE_VERSION || p.joint_count != PC_REMOTE_BODY_JOINTS ||
        !p.scene_epoch || p.reserved || p.stage[8] || !p.stage[0] || p.room < 0 ||
        p.room > 63 || p.layer < 0 || p.layer > 15) return false;
    bool ended = false;
    for (char c : p.stage) {
        if (!c) { ended = true; continue; }
        if (ended || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '_')) return false;
    }
    for (float scale : p.base_scale) if (!std::isfinite(scale) || scale <= 0 || scale > 128) return false;
    for (int i = -1; i < PC_REMOTE_BODY_JOINTS; ++i) {
        const float (*m)[4] = i < 0 ? p.base : p.joints[i];
        if (i >= 0 && p.scale_flags[i] > 1) return false;
        for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c)
            if (!std::isfinite(m[r][c]) || std::fabs(m[r][c]) > (c == 3 ? 10000000.0f : 128.0f)) return false;
    }
    return true;
}

// The original Link.arc is read through native DVD and decoded into a bounded
// scratch heap. No global archive is mounted and local getBinary is never copied:
// its TEX1 entries already point at substituted external toon/clothes textures.
bool fresh_cl_binary(JKRSolidHeap* scratch) {
    CurrentHeap current(scratch);
    JKRDvdFile file;
    if (!file.open("/res/Object/Link.arc")) return false;
    const s32 length = file.getFileSize();
    if (length < 0x40 || u32(length) > kArchiveStoredMax) return false;
    const u32 aligned = (u32(length) + 31) & ~31u;
    u8* stored = static_cast<u8*>(scratch->alloc(aligned, 32));
    if (!stored || DVDReadPrio(file.getFileInfo(), stored, aligned, 0, 2) != s32(aligned)) return false;
    pc_remote_asset::Bytes arc{stored, size_t(length)};
    if (std::memcmp(stored, "Yaz0", 4) == 0) {
        const u32 expanded = pc_remote_asset::be32(stored + 4);
        if (!expanded || expanded > kArchiveExpandedMax) return false;
        u8* unpacked = static_cast<u8*>(scratch->alloc(expanded, 32));
        size_t written = 0;
        if (!unpacked || !pc_remote_asset::yaz0(arc, unpacked, expanded, written)) return false;
        arc = {unpacked, written};
    }
    pc_remote_asset::Bytes cl{};
    if (!pc_remote_asset::cl_resource(arc, dRes_INDEX_LINK_BDL_CL_e, cl)) return false;
    const bool compressed = cl.size >= 16 && std::memcmp(cl.data, "Yaz0", 4) == 0;
    const size_t size = compressed ? pc_remote_asset::be32(cl.data + 4) : cl.size;
    if (!size || size > kBinaryMax) return false;
    sBinary = static_cast<u8*>(sHeap->alloc(u32(size), 32));
    if (!sBinary) return false;
    if (compressed) {
        size_t written = 0;
        if (!pc_remote_asset::yaz0(cl, sBinary, size, written) || written != size) return false;
    } else std::memcpy(sBinary, cl.data, size);
    sBinaryBytes = u32(size);
    return pc_remote_asset::bdl({sBinary, size});
}

ResTIMG* private_toon(ResTIMG* source) {
    dRes_info_c* info = dComIfG_getObjectResInfo("System");
    if (!source || !info || !info->getArchive()) return nullptr;
    const u32 size = info->getArchive()->getExpandedResSize(source);
    if (size < sizeof(ResTIMG) || size > kToonMax) return nullptr;
    const u32 width = source->width, height = source->height;
    if (!width || !height || width > 256 || height > 256 || source->mipmapCount > 8) return nullptr;
    switch (source->format) {
    case GX_TF_I4: case GX_TF_I8: case GX_TF_IA4: case GX_TF_IA8: case GX_TF_RGB565:
    case GX_TF_RGB5A3: case GX_TF_RGBA8: case GX_TF_C4: case GX_TF_C8: case GX_TF_C14X2:
    case GX_TF_CMPR: break;
    default: return nullptr;
    }
    const u32 imageSize = GXGetTexBufferSize(u16(width), u16(height), GXTexFmt(source->format),
                                           source->mipmapEnabled != 0, source->mipmapCount ? source->mipmapCount : 1);
    if (!pc_remote_asset::range(u32(source->imageOffset), imageSize, size) ||
        (source->indexTexture && !pc_remote_asset::range(u32(source->paletteOffset), u32(source->numColors) * 2, size))) return nullptr;
    ResTIMG* result = static_cast<ResTIMG*>(sHeap->alloc(size, 32));
    if (result) std::memcpy(result, source, size);
    return result;
}

// Mirrors d_resorce.cpp:setToonTex against ONLY the owned table/shared DL.
// Both toon BTI payloads are also owned: no scene/archive texture-image lease.
bool init_toon() {
    ResTIMG* a = private_toon(dDlst_list_c::getToonImage());
    ResTIMG* b = private_toon(dDlst_list_c::getToonExImage());
    J3DTexture* tex = sData->getTexture();
    JUTNameTab* names = sData->getTextureName();
    if (!a || !b || !tex || !names) return false;
    for (u16 i = 0; i < tex->getNum(); ++i) {
        const char* name = names->getName(i);
        if (name && name[0] == 'Z' && name[1] == 'A') tex->setResTIMG(i, *a);
        if (name && name[0] == 'Z' && name[1] == 'B') tex->setResTIMG(i, *b);
    }
    j3dSys.setTexture(tex);
    for (u16 i = 0; i < sData->getMaterialNum(); ++i) {
        J3DMaterial* mat = sData->getMaterialNodePointer(i);
        J3DTevBlock* tev = mat->getTevBlock();
        J3DDisplayListObj* dl = mat->getSharedDisplayListObj();
        if (!tev || !dl || !owned(dl->getDisplayList(0), dl->getDisplayListSize())) return false;
        J3DGXColorS10* color = tev->getTevColor(3);
        if (color) color->mColor.a = tev->getTevStageNum();
        const BOOL interrupts = OSDisableInterrupts();
        GDLObj list;
        GDInitGDLObj(&list, dl->getDisplayList(0), dl->getDisplayListSize());
        GDSetCurrent(&list);
        tev->patchTexNoAndTexCoordScale();
        GDSetCurrent(nullptr);
        OSRestoreInterrupts(interrupts);
    }
    return true;
}

bool initialize() {
    if (sAttempted) return sModel != nullptr;
    sAttempted = true;
    J3DState graphics;
    sHeap = mDoExt_createSolidHeapFromSystem(kRendererBytes, 32);
    if (!sHeap) return false;
    sHeap->setErrorFlag(false);
    JKRSolidHeap* scratch = mDoExt_createSolidHeapFromSystem(kScratchBytes, 32);
    if (!scratch) return false;
    scratch->setErrorFlag(false);
    const bool binaryOK = fresh_cl_binary(scratch);
    // Only raw read/decode bytes were put in scratch, never submitted GX/J3D data.
    mDoExt_destroySolidHeap(scratch);
    if (!binaryOK) return false;
    CurrentHeap current(sHeap);
    sData = J3DModelLoaderDataBase::loadBinaryDisplayList(sBinary, 0x00002020);
    if (!sData || sData->getJointNum() != 42 || sData->getMaterialNum() > 64 ||
        sData->getShapeNum() > 64 || sData->getDrawMtxNum() > 256 || sData->getWEvlpMtxNum() > 128) return false;
    sData->getJointTree().setBasicMtxCalc(nullptr);
    for (u16 i = 0; i < 42; ++i) {
        J3DJoint* joint = sData->getJointNodePointer(i);
        joint->setCallBack(nullptr); joint->mCallBackUserData = nullptr;
        joint->setMtxCalc(nullptr); joint->setOldMtxCalc(nullptr);
    }
    for (u16 i = 0; i < sData->getMaterialNum(); ++i) {
        J3DMaterialAnm* anm = new J3DMaterialAnm();
        if (!anm) return false;
        sData->getMaterialNodePointer(i)->setMaterialAnm(anm);
    }
    if (!init_toon()) return false;
    sModel = mDoExt_J3DModel__create(sData, 0x00080000, 0x37221222);
    if (!sModel || sModel->getMtxCalcMode() != 0) { sModel = nullptr; return false; }
    sModel->setUserArea(0);
    // Body-only first slice. No equipment, external hands, eye/brow multipass,
    // facial animation, local clothes swaps, shadows or attachment callbacks.
    const int excluded[] = {CL_JNT_CL_LHANDA_e, CL_JNT_CL_RHANDA_e, CL_JNT_CL_EYE_e,
                            CL_JNT_CL_MAYU_e, CL_JNT_CL_PODA_e, CL_JNT_CL_BACK_e};
    for (int index : excluded)
        for (J3DMaterial* m = sData->getJointNodePointer(index)->getMesh(); m; m = m->getNext()) m->getShape()->hide();
    std::fprintf(stderr, "[cos] remote-body: owned CL ready binary=%u retained_heap=%u used=%u; visual proof pending\n",
                 sBinaryBytes, sHeap->getHeapSize(), sHeap->getHeapSize() - sHeap->getFreeSize());
    return true;
}

bool isolated(J3DModel* local) {
    if (!local || local == sModel || !owned(sModel) || !owned(sData) || !owned(sBinary, sBinaryBytes)) return false;
    J3DModelData* data = local->getModelData();
    if (!data || data == sData || data->getBinary() == sData->getBinary() || data->getJointNum() != 42 ||
        data->getMaterialNum() != sData->getMaterialNum() || data->getShapeNum() != sData->getShapeNum() ||
        data->getTexture() == sData->getTexture() || !owned(sData->getTexture())) return false;
    J3DVertexData& vertices = sData->getVertexData();
    void* arrays[] = {vertices.getVtxPosArray(), vertices.getVtxNrmArray(), vertices.getVtxNBTArray(),
                      vertices.getVtxColorArray(0), vertices.getVtxColorArray(1),
                      vertices.getVtxTexCoordArray(0), vertices.getVtxTexCoordArray(1),
                      vertices.getVtxTexCoordArray(2), vertices.getVtxTexCoordArray(3),
                      vertices.getVtxTexCoordArray(4), vertices.getVtxTexCoordArray(5),
                      vertices.getVtxTexCoordArray(6), vertices.getVtxTexCoordArray(7)};
    if (!owned(vertices.getVtxAttrFmtList()) || sData->getHierarchy() == data->getHierarchy() ||
        !owned(sData->getHierarchy())) return false;
    for (void* array : arrays)
        if (array && (!vertices.getVtxArraySize(array) || !owned(array, vertices.getVtxArraySize(array)))) return false;
    if (!owned(sModel->getMatPacketArray(), sizeof(J3DMatPacket) * sData->getMaterialNum()) ||
        !owned(sModel->getShapePacketArray(), sizeof(J3DShapePacket) * sData->getShapeNum())) return false;
    size_t mixCount = 0;
    for (u16 i = 0; i < sData->getWEvlpMtxNum(); ++i) {
        const u8 count = sData->getWEvlpMixMtxNum(i);
        if (!count || count > 42 || !owned(sModel->getWeightAnmMtx(i), sizeof(Mtx))) return false;
        mixCount += count;
    }
    if (mixCount && (!owned(sData->getWEvlpMixMtxIndex(), mixCount * sizeof(u16)) ||
                     !owned(sData->getWEvlpMixWeight(), mixCount * sizeof(float)))) return false;
    for (size_t i = 0; i < mixCount; ++i) {
        const u16 index = sData->getWEvlpMixMtxIndex()[i];
        if (index >= 42 || !owned(sData->getInvJointMtx(index), sizeof(Mtx))) return false;
    }
    for (u16 i = 0; i < 42; ++i) {
        J3DJoint* j = sData->getJointNodePointer(i);
        if (j == data->getJointNodePointer(i) || !owned(j) || j->getCallBack() || j->getMtxCalc() ||
            j->getOldMtxCalc() || j->mCallBackUserData || !owned(sModel->getAnmMtx(i), sizeof(Mtx)) ||
            sModel->getAnmMtx(i) == local->getAnmMtx(i)) return false;
    }
    for (u16 i = 0; i < sData->getShapeNum(); ++i) {
        J3DShape* shape = sData->getShapeNodePointer(i);
        if (shape == data->getShapeNodePointer(i) || !owned(shape) || !owned(shape->getVcdVatCmd())) return false;
        for (u32 j = 0; j < shape->getMtxGroupNum(); ++j) {
            J3DShapeDraw* draw = shape->getShapeDraw(j);
            if (!owned(draw) || !owned(shape->getShapeMtx(j)) ||
                !owned(draw->getDisplayList(), draw->getDisplayListSize())) return false;
        }
    }
    for (u16 i = 0; i < sData->getMaterialNum(); ++i) {
        J3DMaterial* m = sData->getMaterialNodePointer(i), *l = data->getMaterialNodePointer(i);
        if (m == l || !owned(m) || !owned(m->getColorBlock()) || !owned(m->getTexGenBlock()) ||
            !owned(m->getTevBlock()) || !owned(m->getIndBlock()) || !owned(m->getPEBlock()) ||
            !owned(m->getMaterialAnm()) || !owned(m->getSharedDisplayListObj()) ||
            m->getColorBlock() == l->getColorBlock() || m->getTexGenBlock() == l->getTexGenBlock() ||
            m->getTevBlock() == l->getTevBlock() || m->getPEBlock() == l->getPEBlock() ||
            m->getMaterialAnm() == l->getMaterialAnm() || m->getSharedDisplayListObj() == l->getSharedDisplayListObj() ||
            (m->mpOrigMaterial && !owned(m->mpOrigMaterial))) return false;
        if (m->getFog() && (!owned(m->getFog()) || m->getFog() == l->getFog())) return false;
        J3DDisplayListObj* dl = m->getSharedDisplayListObj();
        if (!owned(dl->getDisplayList(0), dl->getDisplayListSize())) return false;
        for (u32 t = 0; t < 8; ++t) {
            J3DTexMtx* matrix = m->getTexMtx(t);
            if (matrix && (!owned(matrix) || matrix == l->getTexMtx(t))) return false;
        }
    }
    J3DTexture* texture = sData->getTexture();
    for (u16 i = 0; i < texture->getNum(); ++i) {
        ResTIMG* image = texture->getResTIMG(i);
        if (!owned(image, sizeof(*image)) || !u16(image->width) || !u16(image->height) ||
            u16(image->width) > 2048 || u16(image->height) > 2048 || image->mipmapCount > 12) return false;
        const u32 size = GXGetTexBufferSize(image->width, image->height, image->format,
                                          image->mipmapEnabled != 0, image->mipmapCount ? image->mipmapCount : 1);
        if (!size || !owned(reinterpret_cast<u8*>(image) + s32(u32(image->imageOffset)), size) ||
            (image->indexTexture && !owned(reinterpret_cast<u8*>(image) + s32(u32(image->paletteOffset)), u32(image->numColors) * 2))) return false;
    }
    return true;
}
} // namespace

namespace pc_remote_body {
bool prepare_private(J3DModel* local) {
    if (!initialize() || !isolated(local)) { sReady = false; hide(); return false; }
    if (!sReady) std::fprintf(stderr, "[cos] remote-body: private pointer isolation checked; not visual/multiplayer acceptance\n");
    sReady = true;
    return true;
}
bool stage(const PcRemoteBodyPose& pose) {
    if (!sReady || !finite_pose(pose)) { hide(); return false; }
    if (sSceneEpoch && (sSceneEpoch != pose.scene_epoch || sPose.room != pose.room || sPose.layer != pose.layer ||
                        std::memcmp(sPose.stage, pose.stage, sizeof(pose.stage)))) {
        hide(); sSceneEpoch = pose.scene_epoch; sPose = pose; return false;
    }
    sSceneEpoch = pose.scene_epoch;
    sPose = pose;
    sHavePose = true;
    return true;
}
void hide() { sHavePose = false; forget_pose(); }
void before_p1(const PcRemoteBodyPose& receiver) {
    const u32 frame = pc_frame_count();
    if (!sReady || !sHavePose || sSubmittedFrame == frame) return;
    if (receiver.scene_epoch != sPose.scene_epoch || receiver.room != sPose.room || receiver.layer != sPose.layer ||
        std::memcmp(receiver.stage, sPose.stage, sizeof(sPose.stage)) || frame - sPose.sample_frame > 1) { hide(); return; }
    J3DState graphics;
    CurrentHeap current(sHeap);
    // World matrices already include actor transform. Apply diagnostic offset ONCE
    // in capture, then only receiver view here. Never model::calc/update/player draw.
    forget_pose();
    std::memcpy(sModel->getBaseTRMtx(), sPose.base, sizeof(Mtx));
    const Vec scale{sPose.base_scale[0], sPose.base_scale[1], sPose.base_scale[2]};
    sModel->setBaseScale(scale);
    for (int i = 0; i < 42; ++i) {
        std::memcpy(sModel->getAnmMtx(i), sPose.joints[i], sizeof(Mtx));
        sModel->setScaleFlag(i, sPose.scale_flags[i]);
    }
    sModel->calcWeightEnvelopeMtx();
    dKy_tevstr_c lighting;
    dKy_tevstr_init(&lighting, sPose.room, 0xff);
    cXyz position(sPose.base[0][3], sPose.base[1][3], sPose.base[2][3]);
    g_env_light.settingTevStruct(TEV_TYPE_PLAYER, &position, &lighting);
    g_env_light.setLightTevColorType(sModel, &lighting);
    dComIfGd_setListP1();
    J3DDrawBuffer* opa = j3dSys.getDrawBuffer(J3DSysDrawBuf_Opa);
    J3DDrawBuffer* xlu = j3dSys.getDrawBuffer(J3DSysDrawBuf_Xlu);
    MtxP oldOpaZ = opa->getZMtx();
    MtxP oldXluZ = xlu->getZMtx();
    mDoExt_modelEntryDL(sModel); // calcMaterial/diff/entry/viewCalc; no skeletal calc.
    opa->setZMtx(oldOpaZ);
    xlu->setZMtx(oldXluZ);
    forget_pose(); // Hold the submitted exact pose; no matrix interpolation claim.
    sSubmittedFrame = frame;
    if (++sSubmissions == 1 || sSubmissions % 300 == 0)
        std::fprintf(stderr, "[cos] remote-body: submitted=%u frame=%u epoch=%llu retained_bytes=%u; geometry/material inspection pending\n",
                     sSubmissions, frame, static_cast<unsigned long long>(sSceneEpoch), sHeap->getHeapSize());
}
} // namespace pc_remote_body
