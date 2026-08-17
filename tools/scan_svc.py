import struct, sys

path = sys.argv[1]
data = open(path, "rb").read()
svc = bytes.fromhex("010000d4")
print(f"size: {len(data)/1e6:.0f} MB, total svc count: {data.count(svc)}")

sites = {}
i = 0
while True:
    j = data.find(svc, i)
    if j < 0:
        break
    if j >= 4:
        w = struct.unpack("<I", data[j - 4:j])[0]
        # movz w8,#imm16 (sf=0) or movz x8 (sf=1): opcode 0x528/0xD28, Rd=8
        if (w & 0xFFE0001F) == 0x52800008 or (w & 0xFFE0001F) == 0xD2800008:
            n = (w >> 5) & 0xFFFF
            sites.setdefault(n, []).append(j - 4)
    i = j + 1

# nr observed killed by vendor seccomp via probe (untraced)
killed = {291, 383, 384, 385, 386, 387, 388, 389, 390, 391, 392, 393, 403,
          424, 425, 434, 435, 436, 437, 439, 441, 500, 512, 600, 1000}

print("syscall sites by nr (movz w8/x8, #N + svc pattern):")
danger = []
for n in sorted(sites):
    mark = ""
    if n in killed:
        mark = "  <<< KILLED BY ROM"
        danger.append(n)
    print(f"  nr={n}: {len(sites[n])} sites{mark}")

print()
print("DANGER nr present in binary:", danger)
