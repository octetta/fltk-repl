/ Male ay diphthong
/ two formant tables crossfaded: e-like to i-like
/ F0 about 110 Hz, about 400 ms

N: 17640
M: 8820
C: p2%p0

/ table 1: onset e-like (F1~550, F2~1800, F3~2500)
P: ~1024
H: !32
H: H+1
D: H-5
B: H-16
J: H-23
A: (1%H)*(e(0-D*D*.3)+e(0-B*B*.18)*.9+e(0-J*J*.12)*.4)
U: w P $ A

/ table 2: offset i-like (F1~400, F2~2200, F3~2700)
D: H-3.6
B: H-20
J: H-24.5
A: (1%H)*(e(0-D*D*.35)+e(0-B*B*.16)*.95+e(0-J*J*.11)*.45)
V: w P $ A

/ play both at male pitch
S: U t 110 N
R: V t 110 N

/ linear crossfade
T: !N
G: T%N
X: (1-G)*S + G*R

/ spoken envelope
E: (50 u T)*e(T*(0-3.5%N))

W: w X*E
