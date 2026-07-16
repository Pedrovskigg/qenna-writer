#!/usr/bin/env python
# Gera uma textura de oceano com profundidade sintetica: como o heightmap nao tem
# batimetria, estimo a profundidade pela DISTANCIA A COSTA (aguas rasas perto do
# litoral, fundo no meio do oceano). Saida RGBA equiretangular: oceano colorido
# (turquesa raso -> azul-marinho profundo), terra transparente.
import sys
import numpy as np
from PIL import Image, ImageFilter

Image.MAX_IMAGE_PIXELS = None
SRC = sys.argv[1] if len(sys.argv) > 1 else r"C:/Users/pedro/AppData/Local/Temp/dem/gebco.png"
OUT = sys.argv[2] if len(sys.argv) > 2 else "src/assets/geo/ocean.png"
W = int(sys.argv[3]) if len(sys.argv) > 3 else 4096
H = W // 2

print(f"abrindo {SRC} ...")
z = np.asarray(Image.open(SRC).convert("L").resize((W, H), Image.LANCZOS)).astype(np.float32)
land = (z >= 1.0).astype(np.float32)  # 1 = terra, 0 = oceano (oceano = 0 no GEBCO)

# Proximidade da costa: borra a mascara de terra em varios raios e soma. No
# oceano isso decai com a distancia ao litoral -> "rasura" (1 costa, 0 fundo).
landimg = Image.fromarray((land * 255).astype("uint8"))
acc = np.zeros((H, W), np.float32)
wsum = 0.0
for radius, wt in [(3, 0.40), (10, 0.30), (28, 0.20), (64, 0.12)]:
    b = np.asarray(landimg.filter(ImageFilter.GaussianBlur(radius))).astype(np.float32) / 255.0
    acc += b * wt
    wsum += wt
shallow = np.clip(acc / wsum, 0.0, 1.0) * (1.0 - land)
shallow = shallow ** 0.65  # concentra o claro mais perto da costa

deep = np.array([16, 38, 72], np.float32)    # azul-marinho profundo
shal = np.array([74, 146, 172], np.float32)  # turquesa raso
s = shallow[..., None]
rgb = deep[None, None, :] * (1.0 - s) + shal[None, None, :] * s

alpha = (1.0 - land) * 255.0
# Suaviza a borda do alpha pra a transicao terra/mar nao serrilhar.
alpha = np.asarray(Image.fromarray(alpha.astype("uint8")).filter(ImageFilter.GaussianBlur(0.6)))

out = np.dstack([rgb.astype("uint8"), alpha.astype("uint8")[..., None]])
Image.fromarray(out, "RGBA").save(OUT, optimize=True)
print(f"  salvo {OUT}  shallow max={shallow.max():.2f}")
