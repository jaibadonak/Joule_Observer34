"""Bound every path through the loop-free, call-free production ADC ISR.

This checks compiled instruction cycles, not peripheral or interrupt scheduling.
Unknown control flow fails closed so code changes cannot silently bypass it.
"""
import functools
import re
import subprocess
import sys

listing = subprocess.check_output([sys.argv[1], "-d", sys.argv[2]], text=True)
match = re.search(r"^[0-9a-f]+ <__vector_21>:\n(.*?)(?=^[0-9a-f]+ <)",
                  listing, re.M | re.S)
if not match:
    raise SystemExit("ADC vector not found")
instructions = {}
for line in match[1].splitlines():
    row = re.match(r"\s*([0-9a-f]+):\s+((?:[0-9a-f]{2}\s+)+)\s*(\w+)\s*(.*)", line)
    if row:
        address, raw, operation, operand = row.groups()
        instructions[int(address, 16)] = (len(raw.split()), operation, operand)

two_cycle = {"push", "pop", "lds", "sts", "ld", "ldd", "st", "std",
             "adiw", "sbiw", "sbi", "cbi", "mul", "muls", "mulsu"}
one_cycle = {"adc", "add", "and", "andi", "asr", "bclr", "bld", "bset",
             "bst", "clc", "cli", "clr", "com", "cp", "cpc", "cpi", "dec",
             "eor", "in", "inc", "ldi", "lsl", "lsr", "mov", "movw", "neg",
             "nop", "or", "ori", "out", "rol", "ror", "sbc", "sbci", "sec",
             "sei", "ser", "sub", "subi", "swap", "tst"}
visiting = set()


def target(operand):
    destination = re.search(r";\s*0x([0-9a-f]+)", operand)
    if not destination:
        raise ValueError(f"Unrecognised branch: {operand}")
    return int(destination[1], 16)


@functools.lru_cache(None)
def cycles(address):
    if address in visiting:
        raise ValueError("Loop in ADC interrupt; manual timing analysis required")
    visiting.add(address)
    size, op, operand = instructions[address]
    after = address + size
    if op == "reti":
        result = 4
    elif op in {"call", "rcall", "icall", "eicall", "ijmp", "eijmp", "ret"}:
        raise ValueError(f"Unsupported control flow: {op}")
    elif op in {"rjmp", "jmp"}:
        result = (2 if op == "rjmp" else 3) + cycles(target(operand))
    elif op.startswith("br"):
        result = max(1 + cycles(after), 2 + cycles(target(operand)))
    elif op in {"cpse", "sbrc", "sbrs", "sbic", "sbis"}:
        skip_size = instructions[after][0]
        result = max(1 + cycles(after), 1 + skip_size // 2 + cycles(after + skip_size))
    elif op in two_cycle or op in one_cycle:
        result = (2 if op in two_cycle else 1) + cycles(after)
    else:
        raise ValueError(f"Unknown instruction timing: {op}")
    visiting.remove(address)
    return result


# Four-cycle hardware entry and the vector table's three-cycle JMP.
worst = 7 + cycles(min(instructions))
print(f"ADC ISR longest instruction path: {worst}/208 CPU cycles ({worst / 2:.1f}/104 us)")
if worst >= 208:
    raise SystemExit("ADC ISR exceeds the conversion interval")
