"""Read-only EF SP NPC census. Xbox5558 ABI is compiled by verify_coop_npc_layout.

Asynchronous observations; targets and attack commands are not hit/LOS proof.
No writes, AI changes, or process suspension. Intended for diagnostic runs.
"""
import math
import struct

ENTITY_SIZE = 1056
NPC_SIZE = 612
TEAM_BORG = 2

def pointer(v):
    return 0x10000 <= v < 0x80000000 and v % 4 == 0

def read_npcs(read, entities_address, globals_address, level_address):
    if None in (entities_address, globals_address, level_address):
        return None
    def integer(address):
        b = read(address, 4)
        return struct.unpack('<i', b)[0] if len(b) == 4 else None
    root = read(entities_address, 4)
    if len(root) != 4:
        return None
    base = struct.unpack('<I', root)[0]
    count = integer(globals_address + 64)
    if not pointer(base) or count is None or not 1 <= count <= 1024:
        return None
    start = integer(level_address + 12)
    raw = read(base, count * ENTITY_SIZE)
    if len(raw) != count * ENTITY_SIZE:
        return None
    def name(address):
        if not pointer(address):
            return None
        b = read(address, 96)
        return b.split(b'\0', 1)[0].decode('ascii', 'replace') if b else None
    result = []
    rejected = 0
    for number in range(count):
        e = raw[number * ENTITY_SIZE:(number + 1) * ENTITY_SIZE]
        def u(off): return struct.unpack_from('<I', e, off)[0]
        def i(off): return struct.unpack_from('<i', e, off)[0]
        if not i(236) or not u(848):
            continue
        client, npc = u(232), u(848)
        if i(0) != number or not pointer(client) or not pointer(npc):
            rejected += 1
            continue
        c = read(client, 1820)
        n = read(npc, NPC_SIZE)
        if len(c) != 1820 or len(n) != NPC_SIZE:
            rejected += 1
            continue
        pos = struct.unpack_from('<3f', e, 304)
        # Detect freed/replaced entities and an event ring changing during its copy.
        addr = base + number * ENTITY_SIZE
        if (not all(math.isfinite(v) for v in pos)
            or read(addr + 232, 8) != e[232:240]
            or read(addr + 848, 4) != e[848:852]
            or read(client + 112, 4) != c[112:116]):
            rejected += 1
            continue
        enemy = u(584)
        enemy_num = ((enemy - base) // ENTITY_SIZE if base <= enemy < base + count * ENTITY_SIZE
                     and (enemy - base) % ENTITY_SIZE == 0 else None)
        team, enemy_team = struct.unpack_from('<2i', c, 1812)
        result.append(dict(number=number, entity=addr, client=client, npc=npc,
            health=i(540), origin=pos, team=team, enemy_team=enemy_team,
            borg=team == TEAM_BORG, enemy=enemy_num, enemy_pointer=enemy,
            npc_type=name(u(860)), targetname=name(u(480)),
            flags=u(340), sv_flags=u(244), takedamage=i(548),
            behavior=struct.unpack_from('<3i', n, 128),
            enemy_seen=struct.unpack_from('<i', n, 48)[0],
            enemy_visibility=struct.unpack_from('<i', n, 8)[0],
            ai_flags=struct.unpack_from('<I', n, 72)[0],
            script_flags=struct.unpack_from('<I', n, 472)[0],
            shot_time=struct.unpack_from('<i', n, 80)[0],
            next_think=struct.unpack_from('<i', n, 548)[0],
            buttons=struct.unpack_from('<I', n, 556)[0],
            move=struct.unpack_from('<2b', n, 576),
            event_sequence=struct.unpack_from('<I', c, 112)[0],
            events=struct.unpack_from('<2i', c, 116),
            command_time=struct.unpack_from('<i', c, 0)[0],
            weapon=struct.unpack_from('<i', c, 148)[0]))
    if read(entities_address, 4) != root or integer(globals_address + 64) != count:
        return None
    return dict(entity_count=count, npcs=result, rejected=rejected,
                level_time_start=start, level_time_end=integer(level_address + 12), atomic=False)
