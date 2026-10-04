#!/usr/bin/env python3
"""Plain loads and stores in the prepaid block copies without their pc and
cycle suffix stores.

  lean_memory.py COMPOSITE_SRC

fast_blocks.py's copies still store, before every guest load or store, the
instruction's pc and its cycle observation suffix, and read the suffix back
for the deadline test after it. Only an MMIO handler reads either while a
block runs (and only those, by way of the access's out-of-line path, the
timebase and the decrementer, each of which stores its own suffix first), so
for an access to ordinary RAM both stores are dead. Here such an access calls
gather_pipe.h's bw_readN_at_observed / bw_writeN_at_observed, which store the
two on the way out of line only and report whether a callback could run.

The pc and suffix are restored before any subsequent observation and on every
exit, including the ordinary end of the copy. Unlike the donor transform,
this preserves the complete CPU state instead of normalizing the suffix in
comparisons. Slow access paths publish both before invoking an observer;
callback changes to either are retained, including in the deadline refund.

An instruction qualifies when its text makes exactly one guest access call
(mem_readN or mem_writeN) and otherwise calls only the pure bit helpers, and
neither jumps nor returns. The change is repeatable (a prepared chunk is left
as it is) and keeps LF line ends. Run it right after fast_blocks.py: it
changes only the copies, which the certified natives' body hashes do not
cover, and before the steps that hash whole chunk files.
"""
import re
import sys
from pathlib import Path

MARK = "/* bluewake: lean memory accesses in prepaid copies (scripts/windows/lean_memory.py) */\n"
FAST_MARK = "/* bluewake: prepaid block copies (scripts/windows/fast_blocks.py) */\n"
COPY = re.compile(r"^bwfast_(\d+):$")
END = re.compile(r"^    goto bwend_(\d+);$")
PC = re.compile(r"^    ctx->pc = 0x([0-9A-F]{8})u;$")
SUFFIX = re.compile(r"^    ctx->cycle_observation_suffix = (\d+)u;$")
COMMENT = re.compile(r"^    // ([0-9A-F]{8}): ")
ACCESS = re.compile(r"\bmem_(read|write)(8|16|32|64)\(ctx, ")
CALL = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
PURE = {"if", "dolrecomp_rotl32", "dolrecomp_f32_from_bits", "dolrecomp_f64_from_bits",
        "dolrecomp_f32_to_bits", "dolrecomp_f64_to_bits", "dolrecomp_ps_from_bits",
        "dolrecomp_ps_to_bits", "sizeof", "while", "for", "switch", "return",
        "__builtin_bswap16", "__builtin_bswap32", "__builtin_bswap64"}
CHECK = [
    "    if (ctx->cycle_deadline_budget > 0 &&",
    "        (s64)ctx->cycle_observation_suffix > ctx->cycle_deadline_budget) {",
    "        ctx->downcount += (s64)ctx->cycle_observation_suffix;",
    "        cycle_block_prepaid = false;",
]
REFUND_GOTO = re.compile(r"^        goto (bwslow_\d+_\d+|bwend_\d+);$")


def calls(text):
    return [name for name in CALL.findall(text) if name not in PURE]


def leaves(text):
    return "return" in text or "goto" in text


def check_at(lines, i):
    """The fast copy's deadline test at lines[i] (6 lines), or None."""
    if lines[i:i + 4] == CHECK and i + 5 < len(lines) and REFUND_GOTO.match(lines[i + 4]) and lines[i + 5] == "    }":
        return 6
    return None


def parse_copy(lines):
    """A copy's lines (after its label, before its closing goto) as items:
    ('pc', addr) a standalone pc store, ('insn', header_pc, suffix, addr, body, check)
    or ('line', text) anything else."""
    items, i = [], 0
    while i < len(lines):
        j, header_pc, suffix = i, None, None
        if PC.match(lines[j]):
            header_pc = PC.match(lines[j]).group(1)
            j += 1
        if j < len(lines) and SUFFIX.match(lines[j]):
            suffix = int(SUFFIX.match(lines[j]).group(1))
            j += 1
        if j < len(lines) and COMMENT.match(lines[j]):
            addr = COMMENT.match(lines[j]).group(1)
            k = j + 1
            while k < len(lines) and not (PC.match(lines[k]) or SUFFIX.match(lines[k]) or COMMENT.match(lines[k])
                                          or check_at(lines, k)):
                k += 1
            body = lines[j:k]
            check = None
            if k < len(lines) and check_at(lines, k):
                check = lines[k:k + 6]
                k += 6
            items.append(("insn", header_pc, suffix, addr, body, check))
            i = k
            continue
        if header_pc is not None and j == i + 1:
            items.append(("pc", header_pc))
            i = j
            continue
        items.append(("line", lines[i]))
        i += 1
    return items


def lean_copy(lines, first_addr, copy_id="0"):
    """The copy's lines with its plain accesses lean, or None if it holds none."""
    items = parse_copy(lines)
    out, changed = [], 0
    pending = None        # (pc, observed flags since its deferred store)
    pending_suffix = None

    def emit_pending(indent="    "):
        if pending is not None:
            pc, observed = pending
            out.extend([f"{indent}if (!({' || '.join(observed)}))",
                        f"{indent}    ctx->pc = 0x{pc}u;"])
        if pending_suffix is not None:
            suffix, observed = pending_suffix
            out.extend([f"{indent}if (!{observed})",
                        f"{indent}    ctx->cycle_observation_suffix = {suffix}u;"])

    def flush():
        nonlocal pending, pending_suffix
        emit_pending()
        pending = pending_suffix = None

    for item in items:
        if item[0] == "pc":
            out.append(f"    ctx->pc = 0x{item[1]}u;")
            pending = None
            continue
        if item[0] == "line":
            if (calls(item[1]) or leaves(item[1]) or
                    "ctx->pc" in item[1] or "ctx->cycle_observation_suffix" in item[1]):
                flush()
            out.append(item[1])
            continue
        _, header_pc, suffix, addr, body, check = item
        text = "\n".join(body)
        accesses = ACCESS.findall(text)
        others = [name for name in calls(text) if not re.fullmatch(r"mem_(read|write)(8|16|32|64)", name)]
        lean = (suffix is not None and check is not None and len(accesses) == 1 and not others
                and not leaves(text) and "ctx->pc" not in text
                and "ctx->cycle_observation_suffix" not in text
                and not re.search(r"\b(?:for|while|do|switch)\b", text))
        if lean:
            if header_pc is not None:
                pc = f"0x{header_pc}u"
            elif pending is not None:
                deferred, flags = pending
                pc = f"({' || '.join(flags)}) ? ctx->pc : 0x{deferred}u"
            else:
                pc = "ctx->pc"
            observed = f"bwlean_observed_{copy_id}_{changed}"
            refund = f"bwlean_refund_{copy_id}_{changed}"
            # A statement before the declaration keeps labels valid in C11.
            out.extend(["    ;", f"    bool {observed} = false;"])
            kind, bits = accesses[0]
            out.extend(ACCESS.sub(f"bw_{kind}{bits}_at_observed(ctx, ({pc}), {suffix}u, &{observed}, ", line)
                       for line in body)
            if header_pc is not None:
                pending = (header_pc, [observed])
            elif pending is not None:
                pending = (pending[0], [*pending[1], observed])
            pending_suffix = (suffix, observed)
            target = REFUND_GOTO.match(check[4]).group(1)
            out.extend([
                f"    const u32 {refund} = {observed} ? ctx->cycle_observation_suffix : {suffix}u;",
                "    if (ctx->cycle_deadline_budget > 0 &&",
                f"        (s64){refund} > ctx->cycle_deadline_budget) {{",
            ])
            emit_pending("        ")
            out.extend([
                f"        ctx->downcount += (s64){refund};",
                "        cycle_block_prepaid = false;",
                f"        goto {target};",
                "    }",
            ])
            changed += 1
            continue
        if header_pc is not None:
            out.append(f"    ctx->pc = 0x{header_pc}u;")
            pending = None
        if (calls(text) or leaves(text) or check is not None or
                "ctx->pc" in text or "ctx->cycle_observation_suffix" in text):
            flush()
        if suffix is not None:
            out.append(f"    ctx->cycle_observation_suffix = {suffix}u;")
            pending_suffix = None
        out.extend(body)
        if check is not None:
            flush()
            out.extend(check)
    flush()
    return out if changed else None, changed


def transform(text):
    if MARK in text:
        return text, 0
    if FAST_MARK not in text:
        return text, 0
    lines = text.split("\n")
    out, i, total = [], 0, 0
    while i < len(lines):
        m = COPY.match(lines[i])
        if not m:
            out.append(lines[i])
            i += 1
            continue
        n = m.group(1)
        j = i + 1
        while j < len(lines) and not (END.match(lines[j]) and END.match(lines[j]).group(1) == n):
            j += 1
        if j >= len(lines):
            raise ValueError(f"copy bwfast_{n} has no end")
        body = lines[i + 1:j]
        first = next((COMMENT.match(l).group(1) for l in body if COMMENT.match(l)), None)
        lean, count = (None, 0) if first is None else lean_copy(body, first, n)
        out.append(lines[i])
        out.extend(lean if lean is not None else body)
        out.append(lines[j])
        total += count
        i = j + 1
    converted = "\n".join(out)
    if total:
        converted = converted.replace(FAST_MARK, FAST_MARK + MARK, 1)
    return converted, total


def main():
    root = Path(sys.argv[1])
    chunks = sorted(root.glob("chunks_*/*.c"))
    if not chunks:
        sys.exit(f"no chunks under {root}")
    accesses = files = 0
    for path in chunks:
        with open(path, encoding="utf-8", newline="") as file:
            original = file.read()
        converted, count = transform(original)
        if count:
            temporary = path.with_suffix(".c.tmp")
            with open(temporary, "w", encoding="utf-8", newline="") as file:
                file.write(converted)
            temporary.replace(path)
            accesses += count
            files += 1
    print(f"lean memory accesses: {accesses} in {files} chunks")


if __name__ == "__main__":
    main()
