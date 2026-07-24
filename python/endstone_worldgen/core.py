from __future__ import annotations
from dataclasses import dataclass
from enum import Enum,IntEnum,auto
from concurrent.futures import ThreadPoolExecutor,Future
import hashlib,struct
@dataclass(frozen=True,slots=True)
class ChunkPos:x:int;z:int
class Stage(Enum):BIOMES=auto();BASE_TERRAIN=auto();SURFACE=auto();CARVERS=auto();STRUCTURES=auto();FEATURES=auto();DECORATION=auto();BLOCK_ENTITIES=auto();FINALIZATION=auto()
class Priority(IntEnum):BACKGROUND=10;FORCED=30;VIEW_DISTANCE=60;TELEPORT=80;PLAYER_REQUESTED=100
@dataclass(slots=True)
class GenerationContext:world_seed:int;dimension:str;chunk:ChunkPos;stage:Stage=Stage.BASE_TERRAIN;stage_seed:int=0

def stage_seed(c:GenerationContext)->int:
    raw=f"{c.world_seed}|{c.dimension}|{c.chunk.x}|{c.chunk.z}|{c.stage.name}".encode();return int.from_bytes(hashlib.blake2b(raw,digest_size=8).digest(),"big")
class ChunkBuffer:
    def __init__(self,pos:ChunkPos,min_y=-64,max_y=319,fill=0):self.pos=pos;self.min_y=min_y;self.max_y=max_y;self._b=[fill]*(16*16*(max_y-min_y+1));self.biomes={}
    def _i(self,x,y,z):
        if not(0<=x<16 and 0<=z<16 and self.min_y<=y<=self.max_y):raise IndexError((x,y,z))
        return (y-self.min_y)*256+z*16+x
    def get(self,x,y,z):return self._b[self._i(x,y,z)]
    def set(self,x,y,z,v):self._b[self._i(x,y,z)]=v
    def fill(self,ax,ay,az,bx,by,bz,v):
        for x in range(max(0,min(ax,bx)),min(15,max(ax,bx))+1):
          for y in range(max(self.min_y,min(ay,by)),min(self.max_y,max(ay,by))+1):
           for z in range(max(0,min(az,bz)),min(15,max(az,bz))+1):self.set(x,y,z,v)
    def fingerprint(self):
        h=hashlib.blake2b(digest_size=8)
        for v in self._b:h.update(struct.pack("<I",v))
        return int.from_bytes(h.digest(),"big")
class FlatGenerator:
    identifier="endstone:flat"
    def __init__(self,surface_y=64,base=1,top=2):self.surface_y=surface_y;self.base=base;self.top=top
    def generate(self,c,b):b.fill(0,b.min_y,0,15,self.surface_y-1,15,self.base);b.fill(0,self.surface_y,0,15,self.surface_y,15,self.top)
class GenerationScheduler:
    def __init__(self,workers=4):self.pool=ThreadPoolExecutor(max_workers=max(1,workers),thread_name_prefix="worldgen")
    def generate(self,c:GenerationContext,g)->Future:
        def run():c.stage_seed=stage_seed(c);b=ChunkBuffer(c.chunk);g.generate(c,b);return b
        return self.pool.submit(run)
    def close(self):self.pool.shutdown(wait=True,cancel_futures=True)
