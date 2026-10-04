#ifndef BLUEWAKE_PORPOISE_MTX_H
#define BLUEWAKE_PORPOISE_MTX_H

/* libPorpoise's native matrix source slice. Only host-endian floats cross
 * this boundary; guest pointers and the CPU ABI remain in native_math.c. */
#ifdef __cplusplus
extern "C" {
#endif

void bluewake_porpoise_mtx_identity(float matrix[3][4]);
void bluewake_porpoise_mtx_trans(float matrix[3][4], float x, float y, float z);
void bluewake_porpoise_mtx_scale(float matrix[3][4], float x, float y, float z);

#ifdef __cplusplus
}
#endif

#endif
