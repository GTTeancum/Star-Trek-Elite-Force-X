"""Read-only SP/co-op player observations using the compiled Xbox5558 SP layout.

Offsets match sp_player_layout.cpp/.asm and the existing paused player reader.
Observations are asynchronous, not a per-frame atomic snapshot or input proof.
"""
import math
import struct


def read_players(read, entities_address, p2_address):
    if entities_address is None or p2_address is None:
        return None  # MP/old maps without the SP symbols are not this layout.
    root = read(entities_address, 4)
    p2_raw = read(p2_address, 4)
    if len(root) != 4 or len(p2_raw) != 4:
        return None
    entities, = struct.unpack('<I', root)
    p2, = struct.unpack('<I', p2_raw)
    def pointer(value):
        return 0x10000 <= value < 0x80000000 and value % 4 == 0
    if not pointer(entities) or not 0 < p2 < 1024:
        return None
    players = []
    for number in (0, p2):
        address = entities + number * 1056
        entity = read(address, 1056)
        if len(entity) != 1056 or struct.unpack_from('<i', entity)[0] != number:
            return None
        client, = struct.unpack_from('<I', entity, 232)
        if not pointer(client):
            return None
        # EF SP layout, independently compiled with Xbox5558 (not JA's
        # similarly named g_local.h): event IDs are 21/22, not JA's 28/29.
        # Include all four ammo counters for actual combat-workload evidence.
        state = read(client, 396)
        if len(state) != 396:
            return None
        origin = struct.unpack_from('<3f', state, 20)
        angles = struct.unpack_from('<3f', state, 156)
        if not all(math.isfinite(v) for v in origin + angles):
            return None
        if read(client + 112, 4) != state[112:116]:
            return None  # Event ring advanced while copying this observation.
        # Detect entity/client replacement across a map load or P2 respawn.
        if read(address + 232, 4) != struct.pack('<I', client):
            return None
        players.append(dict(number=number, entity=address, client=client,
            command_time=struct.unpack_from('<i', state)[0], origin=origin,
            viewangles=angles, health=struct.unpack_from('<i', state,188)[0],
            event_sequence=struct.unpack_from('<I', state,112)[0],
            events=struct.unpack_from('<2i', state,116),
            weapon=struct.unpack_from('<i', state,148)[0],
            weapon_time=struct.unpack_from('<i', state,44)[0],
            weapon_state=struct.unpack_from('<i', state,152)[0],
            ammo=struct.unpack_from('<4i', state,380),
            entity_health=struct.unpack_from('<i', entity,540)[0]))
    if read(entities_address,4) != root or read(p2_address,4) != p2_raw:
        return None
    return players
