from endstone_worldgen import *
scheduler=GenerationScheduler(4)
ctx=GenerationContext(12345,"overworld",ChunkPos(0,0))
chunk=scheduler.generate(ctx,FlatGenerator(surface_y=80,base=1,top=2)).result()
print("fingerprint",chunk.fingerprint(),"surface",chunk.get(0,80,0))
scheduler.close()
