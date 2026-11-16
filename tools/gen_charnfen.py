# Charnfen: a small Gothic cathedral (altar to the north), its churchyard, and the square with the great fire.
# Usage: gen_charnfen.py [hero x y] > ref/charnfen.txt
import math, random, sys
W,H=120,64
g=[['.']*W for _ in range(H)]
def put(x,y,c): g[y][x]=c
inside=set()
def floor(x,y): put(x,y,'_'); inside.add((x,y))
cx=60
random.seed(11)
# ---- the cathedral: a Latin cross turned with its portal to the square
for y in range(9,35):                       # choir and nave, aisles
    for x in range(52,69):
        if x in(52,68): put(x,y,'#')
        else: floor(x,y)
for y in range(15,22):                      # transept
    for x in range(45,76):
        if y in(15,21) and not 52<x<68 or x in(45,75): put(x,y,'#')
        else: floor(x,y)
for x in range(53,68): floor(x,15); floor(x,21)
for y in range(16,21): floor(52,y); floor(68,y)
# apse with three chapels
chap=[(cx+7*math.cos(math.radians(a)), 9+7*math.sin(math.radians(a))) for a in (-150,-90,-30)]
for y in range(1,12):
    for x in range(45,76):
        if y<=9 and (math.hypot(x-cx,y-9)<5.8 or any(math.hypot(x-a,y-b)<2.1 for a,b in chap)): floor(x,y)
for y in range(1,13):
    for x in range(44,77):
        if (x,y) not in inside and any((x+dx,y+dy) in inside for dx in(-1,0,1) for dy in(-1,0,1)): put(x,y,'#')
for x in range(53,68): floor(x,9)
# towers and narthex at the south front
for (x0,x1) in ((47,55),(65,73)):
    for y in range(33,41):
        for x in range(x0,x1+1):
            if x in(x0,x1) or y in(33,40): put(x,y,'#')
            else: floor(x,y)
for y in range(35,40):
    for x in range(56,65): floor(x,y)
for x in range(55,66): put(x,40,'#')
for x in range(56,65): put(x,34,'_')
put(55,37,'+'); put(65,37,'+')
for x in (59,60,61): put(x,40,'+')            # the portal
put(45,18,'+'); put(75,18,'+')                # transept doors
# pillars, pews, choir stalls, the altar and the crypt stair before it
for y in range(23,33,3): put(55,y,'o'); put(65,y,'o')
for x,y in ((54,16),(66,16),(54,20),(66,20)): put(x,y,'o')
for y in range(23,33):
    if y%3==2: continue
    for x in (57,58,59,61,62,63): put(x,y,'≡')
for y in range(10,14): put(56,y,'≡'); put(64,y,'≡')
put(cx,6,'‡'); put(cx,12,'>')
for x in (58,62): put(x,6,'i')                # candles beside the altar
for a,b in chap: put(round(a),round(b),'‡')
for x,y in ((47,17),(47,19),(73,17),(73,19),(54,10),(66,10),(57,8),(63,8)): put(x,y,'i')
for x,y in ((53,24),(67,24),(53,30),(67,30)): put(x,y,'i')
for y in range(24,33,3): put(52,y,'w'); put(68,y,'w')
for y in (10,12): put(52,y,'w'); put(68,y,'w')
for x in (48,72): put(x,15,'w'); put(x,21,'w')
for y in range(H):
    for x in range(44,77):
        if y<9 and g[y][x]=='#' and (x+y)%3==0: put(x,y,'w')
# ---- churchyard: walled, full of the dead
X0,X1,YW=30,90,42
for y in range(1,YW):
    put(X0,y,'#'); put(X1,y,'#')
for x in range(X0,X1+1):
    if not 52<=x<=68: put(x,YW,'#')
def busy(x,y): return any(g[yy][xx]!='.' for yy in range(y-1,y+2) for xx in range(x-1,x+2))
def mausoleum(x0,y0):
    for y in range(y0,y0+4):
        for x in range(x0,x0+6): put(x,y,'#' if x in(x0,x0+5) or y in(y0,y0+3) else '_')
    put(x0+2,y0+3,'+')
for x0,y0 in ((33,4),(81,6),(35,30)): mausoleum(x0,y0)
for y in range(1,YW):
    for x in range(X0+1,X1):
        if g[y][x]!='.' or busy(x,y): continue
        if y%2==1 and (x+(y//2)%2)%3==0 and random.random()<.75: put(x,y,'t' if random.random()<.3 else 'n')
        elif random.random()<.02: put(x,y,'♣')
for x,y in ((52,41),(68,41),(X0,YW),(X1,YW)): put(x,y,'T')    # torches at the churchyard gate
# ---- the square with the great fire
SX0,SX1,SY0,SY1=38,82,43,57
for y in range(SY0,SY1+1):
    for x in range(SX0,SX1+1):
        if ((x-cx)/23)**2+((y-50)/8.2)**2<1: put(x,y,':')
for y in range(48,53):                      # the fire, 1-3-1, in a ring of hearth stones
    for x in range(57,64):
        if abs(x-cx)+abs(y-50)<=1: put(x,y,'F')
        elif math.hypot((x-cx)/1.4,y-50)<2.3: put(x,y,'r')
# folk round the fire, and benches of split logs
put(53,49,'h'); put(67,49,'b'); put(55,54,'g'); put(65,54,'$')
for x in (56,57,58): put(x,46,'≡')
for x in (62,63,64): put(x,46,'≡')
# the inn on the west side of the square
for y in range(44,55):
    for x in range(36,48):
        if x in(36,47) or y in(44,54): put(x,y,'#')
        else: floor(x,y)
for y in range(45,54): put(41,y,'#')
put(41,49,'+'); put(47,49,'+')
for y in (46,52): put(47,y,'L')
put(36,47,'L'); put(36,51,'L')
put(39,44,'C'); put(44,44,'C')
for x in range(43,47): put(x,46,'≡')
for x in range(43,47): put(x,52,'≡')
put(38,46,'■') if False else None
put(48,47,'?')                                   # the board by the inn door
put(48,51,'T')
# the Ember Altar on the east side, cold, warm cracks round it
put(78,50,'E')
for x,y in ((76,48),(80,52),(77,53),(79,47)): put(x,y,'%')
for x,y in ((74,46),(74,54),(52,57),(68,57),(50,43),(70,43)): put(x,y,'T')
# the road out, south, into the dead wood
for y in range(57,H-1):
    for x in (59,60,61): put(x,y,':')
for y in range(1,H-1):
    for x in range(1,W-1):
        if g[y][x]=='.' and not (X0<=x<=X1 and y<=YW) and random.random()<.06 and not busy(x,y): put(x,y,'♣')
for x in range(W): put(x,0,'#'); put(x,H-1,'#')
for y in range(H): put(0,y,'#'); put(W-1,y,'#')
hx,hy=(int(sys.argv[1]),int(sys.argv[2])) if len(sys.argv)>2 else (60,59)
put(hx,hy,'@')
print('\n'.join(''.join(r) for r in g))
