import sys
CHUNK=16; PAGE=8192; PER=PAGE//CHUNK   # 512 hexmap bytes per page
hx=open('/home/user/ZiguratIP/home/data/hexmap','rb').read()
pages=len(hx)//PER
print("pages:", pages)
bad=0
for p in range(pages):
    b=hx[p*PER:(p+1)*PER]
    # first 3 chunks are the page header
    body=b[3:]
    first_free=None
    for i,v in enumerate(body):
        if (v & 128)==0:
            first_free=i; break
    if first_free is None: continue
    after=body[first_free:]
    live=[i for i,v in enumerate(after) if (v & 128)!=0]
    if live:
        bad+=1
        print("  page %d (offset %d): first free chunk at %d, but %d allocated chunks come after it"
              % (p, p*PAGE, first_free+3, len(live)))
print("pages whose walk would stop early:", bad)
