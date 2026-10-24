"""Raw chunk helpers for compiled ModelScript (.MSB). zenkit can only READ .MSB, so we write chunks ourselves.
ANI chunk 0xF520 payload: name'\n' u32 layer, next'\n', f32 blendIn, f32 blendOut, flags'\n' (e.g. 'M.'),
  ascName'\n', dir'\n' ('F'/'R'), i32 firstFrame, i32 lastFrame, f32 maxFps, f32 speed, f32 collisionVolumeScale
Optional event block after an ANI: 0xF5A0 (u32 count) + event chunks + 0xF5AF."""
import struct
ANI, ANI_ENUM_END = 0xF520, 0xF5FF
def chunk(t, body): return struct.pack('<HI', t, len(body)) + body
def ani_chunk(name, layer, nxt, blend_in, blend_out, flags, asc, first, last, direction='F', fps=25.0, speed=0.0, cvs=1.0):
    s=lambda x: x.encode('latin1')+b'\n'
    body = s(name)+struct.pack('<I',layer)+s(nxt)+struct.pack('<ff',blend_in,blend_out)+s(flags)+s(asc)+s(direction)
    body += struct.pack('<iifff', first, last, fps, speed, cvs)
    return chunk(ANI, body)
def insert_before_enum_end(msb, extra):
    o=0
    while o < len(msb):
        t,l=struct.unpack_from('<HI',msb,o)
        if t==ANI_ENUM_END: return msb[:o]+extra+msb[o:]
        o+=6+l
    raise ValueError('ANI_ENUM_END not found')
