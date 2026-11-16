# Charnfen: a Gothic cathedral on a Latin cross, its churchyard, and a few homes. Usage: gen_charnfen.py [hero x y]
import math, random, sys
W,H=120,64
g=[['.']*W for _ in range(H)]
def put(x,y,c): g[y][x]=c
inside=set()
def floor(x,y): put(x,y,'_'); inside.add((x,y))
def rect(x0,y0,x1,y1):
    for y in range(y0,y1+1):
        for x in range(x0,x1+1):
            if x in(x0,x1) or y in(y0,y1): put(x,y,'#')
            else: floor(x,y)
cy=22
N0,N1=20,64          # nave and aisles
T0,T1=64,77          # transept
C1=93                # choir ends, apse begins
Y0,Y1=13,31          # outer walls of the aisles
# body: nave, choir, crossing
for x in range(N0,C1+1):
    for y in range(Y0,Y1+1):
        if y in(Y0,Y1): put(x,y,'#')
        else: floor(x,y)
# transept arms
for x in range(T0,T1+1):
    for y in range(3,42):
        if y in(3,41) or (x in(T0,T1) and not Y0<y<Y1): put(x,y,'#')
        elif not (Y0<y<Y1 and x in(T0,T1)): floor(x,y)
for y in range(Y0+1,Y1): floor(T0,y); floor(T1,y)
# apse with ambulatory and five radiating chapels (a chevet)
R=8.6
chap=[(C1+11*math.cos(math.radians(a)), cy+11*math.sin(math.radians(a))) for a in (-72,-36,0,36,72)]
for y in range(H):
    for x in range(C1,W-1):
        if math.hypot(x-C1,y-cy)<R or any(math.hypot(x-a,y-b)<2.7 for a,b in chap): floor(x,y)
for y in range(1,H-1):
    for x in range(C1-1,W-1):
        if (x,y) not in inside and any((x+dx,y+dy) in inside for dx in(-1,0,1) for dy in(-1,0,1)): put(x,y,'#')
# west front: two towers, a narthex, the great portal
rect(9,6,N0,Y0+1); rect(9,Y1-1,N0,38)
for y in range(Y0+1,Y1): put(9,y,'#')
for y in range(Y0+2,Y1-1):
    for x in range(10,N0): floor(x,y)
for y in range(Y0+2,Y1-1): put(N0,y,'_' if Y0+5<=y<=Y1-5 else '#')
for y in range(cy-2,cy+3): put(9,y,'+')
put(14,Y0+1,'+'); put(14,Y1-1,'+')

# transept portals and rose windows
for x in (69,70,71,72): put(x,3,'+'); put(x,41,'+')
for x in (66,67,74,75): put(x,3,'w'); put(x,41,'w')
for y in range(5,Y0,3): put(T0,y,'w'); put(T1,y,'w')
for y in range(Y1+2,41,3): put(T0,y,'w'); put(T1,y,'w')
# arcades: pillars between nave and aisles, on through the choir; crossing piers; the ring round the altar
for x in range(N0+3,C1-4,3):
    if T0-1<=x<=T1+1: continue
    put(x,17,'o'); put(x,27,'o')
for x,y in ((T0+1,17),(T1-1,17),(T0+1,27),(T1-1,27),(T0+1,8),(T1-1,8),(T0+1,36),(T1-1,36)): put(x,y,'o')
for a in range(-90,91,30):
    put(round(C1+5.2*math.cos(math.radians(a))), round(cy+5.2*math.sin(math.radians(a))),'o')
# pews in the nave with a walk down the middle; candles at the ends
for x in range(N0+2,T0-2):
    if x%4==0: continue
    for y in (18,19,20,21,23,24,25,26): put(x,y,'≡')
# choir stalls, rood screen gap, the high altar, the crypt stair before it
for x in range(T1+3,C1-3):
    for y in (18,19,25,26): put(x,y,'≡')
put(C1+1,cy,'‡')
put(C1-5,cy,'>')
for a,b in chap: put(round(a),round(b),'‡')
# tombs of bishops in the aisles
for x in range(N0+4,T0-2,8): put(x,15,'n'); put(x,29,'n')
# stained glass
for x in range(N0+2,C1,3):
    if T0<=x<=T1: continue
    put(x,Y0,'w'); put(x,Y1,'w')
for y in range(H):
    for x in range(C1+1,W-1):
        if g[y][x]=='#' and (x*7+y*3)%4==0: put(x,y,'w')
# the churchyard: rows of the dead all round
random.seed(7)
def busy(x,y): return any(g[yy][xx]!='.' for yy in range(max(0,y-1),min(H,y+2)) for xx in range(max(0,x-1),min(W,x+2)))
def mausoleum(x0,y0,door_down):
    for y in range(y0,y0+4):
        for x in range(x0,x0+6): put(x,y,'#' if x in(x0,x0+5) or y in(y0,y0+3) else '_')
    put(x0+2,y0+3 if door_down else y0,'+')
for (x0,y0,d) in ((30,4,True),(44,5,True),(34,38,False),(52,39,False),(100,40,False),(104,3,True)): mausoleum(x0,y0,d)
for y in range(1,47):
    for x in range(1,W-1):
        if g[y][x]!='.' or busy(x,y): continue
        if y%2==1 and (x+(y//2)%2)%3==0 and random.random()<.7: put(x,y,'t' if random.random()<.3 else 'n')
        elif random.random()<.015: put(x,y,'♣')
# churchyard wall with one gate, and the path to the great portal
for x in range(W): put(x,47,'#')
for y in range(cy,60): put(4,y,':')
for x in range(1,9): put(x,cy,':')
for x in range(1,9): put(x,cy-1,'.'); put(x,cy+1,'.')
for y in range(cy+2,47):
    for x in (3,5):
        if g[y][x] in 'nt': put(x,y,'.')
for x in (3,4,5): put(x,47,':')
# Charnfen: a handful of homes round the warm well
def house(x0,y0,w,h,door,lit):
    for y in range(y0,y0+h+1):
        for x in range(x0,x0+w+1): put(x,y,'#' if x in(x0,x0+w) or y in(y0,y0+h) else '_')
    put(*door,'+')
    for p in lit: put(*p,'L')
house(14,51,9,5,(18,56),[(16,51),(21,51)])
house(52,50,8,5,(56,55),[(54,50),(58,50)])
house(30,54,7,4,(33,54),[(37,56)])
for x in range(5,66):
    if g[58][x]=='.': put(x,58,':')
for y in range(53,58): put(44,y,':')
put(44,54,'O')                       # the warm well
put(48,55,'E')                       # the Ember Altar, cold
put(40,56,'?')                       # the notice board
put(48,57,'$')                       # the trader
for x,y in ((43,51),(46,60),(50,52),(68,55),(25,60),(9,52),(30,50)): put(x,y,'%')
for y in range(51,61):
    for x in range(78,104):
        if ((x-90)/11)**2+((y-55.5)/4)**2<1: put(x,y,'≈')
for x in range(W): put(x,0,'#'); put(x,H-1,'#')
for y in range(H): put(0,y,'#'); put(W-1,y,'#')
hx,hy=(int(sys.argv[1]),int(sys.argv[2])) if len(sys.argv)>2 else (6,58)
put(hx,hy,'@')
print('\n'.join(''.join(r) for r in g))
