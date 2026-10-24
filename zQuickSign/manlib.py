"""Minimal reader/writer for ZenGin compiled model animations (.MAN).
Layout (all little-endian), sequence of chunks: u16 id, u32 length, payload.
  0xA000 MARKER   len 0
  0xA020 HEADER   u16 version(=12); name '\n'; u32 layer; u32 frameCount; u32 nodeCount;
                  f32 fps; f32 fpsSource; f32 samplePosRangeMin; f32 samplePosScale;
                  f32 bbox[6] (min xyz, max xyz); next '\n'
  0xA010 SOURCE   date{u32 year; u16 month,day,hour,minute,second,pad}; srcPath '\n'; srcScript '\n'
  0xA030 EVENTS   u32 count; count * event
  0xA090 RAWDATA  u32 checksum (== MDH checksum); u32 nodeIndex[nodeCount];
                  samples[frameCount][nodeCount] each 12 bytes:
                     u16 rot[3]  (quat xyz: (v-32767)*(2.1/65535); w = sqrt(1-x^2-y^2-z^2))
                     u16 pos[3]  (pos = v*samplePosScale + samplePosRangeMin)
"""
import struct
CH_MARKER, CH_SOURCE, CH_HEADER, CH_EVENTS, CH_RAW = 0xA000, 0xA010, 0xA020, 0xA030, 0xA090

def chunks(d):
    o=0
    while o < len(d):
        t,l=struct.unpack_from('<HI',d,o); yield t, o, d[o+6:o+6+l]; o+=6+l

def _line(b,q):
    p=b.index(b'\n',q); return b[q:p].decode('latin1'), p+1

class Man:
    def __init__(self, data):
        self.order=[]; self.raw={}
        for t,o,b in chunks(data):
            self.order.append(t); self.raw[t]=b
        h=self.raw[CH_HEADER]
        self.version,=struct.unpack_from('<H',h,0)
        self.name,q=_line(h,2)
        (self.layer,self.frames,self.nnodes,self.fps,self.fps_src,self.pmin,self.pscale)=struct.unpack_from('<IIIffff',h,q); q+=28
        self.bbox=list(struct.unpack_from('<6f',h,q)); q+=24
        self.next,q=_line(h,q)
        r=self.raw[CH_RAW]
        self.checksum,=struct.unpack_from('<I',r,0)
        self.nodes=list(struct.unpack_from(f'<{self.nnodes}I',r,4))
        off=4+4*self.nnodes
        n=self.frames*self.nnodes
        self.samples=[struct.unpack_from('<6H',r,off+12*i) for i in range(n)]  # raw u16 (rx,ry,rz,px,py,pz)
        assert off+12*n==len(r), (off+12*n, len(r))
    def sample(self, frame, k):
        return self.samples[frame*self.nnodes+k]
    def decode(self, s):
        S=2.1/65535.0; x,y,z=[(v-32767)*S for v in s[:3]]
        l=x*x+y*y+z*z
        if l>1.0: f=l**-0.5; x,y,z,w=x*f,y*f,z*f,0.0
        else: w=(1.0-l)**0.5
        return (x,y,z,w), tuple(v*self.pscale+self.pmin for v in s[3:])
    def events(self):
        b=self.raw.get(CH_EVENTS)
        return struct.unpack_from('<I',b,0)[0] if b else 0

    def build(self):
        h=struct.pack('<H',self.version)+self.name.encode('latin1')+b'\n'
        h+=struct.pack('<IIIffff',self.layer,self.frames,self.nnodes,self.fps,self.fps_src,self.pmin,self.pscale)
        h+=struct.pack('<6f',*self.bbox)+self.next.encode('latin1')+b'\n'
        r=struct.pack('<I',self.checksum)+struct.pack(f'<{self.nnodes}I',*self.nodes)
        r+=b''.join(struct.pack('<6H',*s) for s in self.samples)
        out=b''
        for t in self.order:
            body = h if t==CH_HEADER else r if t==CH_RAW else self.raw[t]
            out+=struct.pack('<HI',t,len(body))+body
        return out

    def subset(self, keep_node_idx, frame_from=0, frame_to=None, name=None, layer=None, nxt=None, drop_events=True, source_script=None):
        """Keep only nodes whose MDH index is in keep_node_idx (order preserved as in source), optional frame slice."""
        frame_to = self.frames-1 if frame_to is None else frame_to
        ks=[k for k,ni in enumerate(self.nodes) if ni in keep_node_idx]
        new=[self.samples[f*self.nnodes+k] for f in range(frame_from,frame_to+1) for k in ks]
        self.nodes=[self.nodes[k] for k in ks]; self.nnodes=len(ks)
        self.samples=new; self.frames=frame_to-frame_from+1
        if name: self.name=name
        if layer is not None: self.layer=layer
        if nxt is not None: self.next=nxt
        if drop_events and CH_EVENTS in self.raw:
            self.order=[t for t in self.order if t!=CH_EVENTS]
        if source_script is not None and CH_SOURCE in self.raw:
            b=self.raw[CH_SOURCE]; date=b[:16]; path,q=_line(b,16)
            self.raw[CH_SOURCE]=date+path.encode('latin1')+b'\n'+source_script.encode('latin1')+b'\n'
        return self

def concat(parts, name, layer, nxt=''):
    """Concatenate Man objects with identical node lists and fps; positions are re-quantized to a common range."""
    a=parts[0]; assert all(p.nodes==a.nodes and p.fps==a.fps for p in parts)
    dec=[p.decode(s)[1] for p in parts for s in p.samples]
    lo=min(min(v) for v in dec); hi=max(max(v) for v in dec); scale=(hi-lo)/65535.0 or 1e-6
    out=Man.__new__(Man); out.__dict__.update({k:(list(v) if isinstance(v,list) else (dict(v) if isinstance(v,dict) else v)) for k,v in a.__dict__.items()})
    out.samples=[]
    for p in parts:
        for s in p.samples:
            pos=p.decode(s)[1]
            out.samples.append(tuple(s[:3])+tuple(max(0,min(65535,round((v-lo)/scale))) for v in pos))
    out.frames=sum(p.frames for p in parts); out.pmin=lo; out.pscale=scale
    out.name=name; out.layer=layer; out.next=nxt
    return out
