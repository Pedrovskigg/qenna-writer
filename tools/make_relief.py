#!/usr/bin/env python
# Gera uma textura de relevo (hillshade) equiretangular a partir do heightmap
# GEBCO da NASA (dominio publico). Saida em escala de cinza centrada no neutro
# (128 = plano), pra ser aplicada como multiply sobre a terra no app.
import sys
import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None  # libera a imagem gigante (21600x10800)

SRC = sys.argv[1] if len(sys.argv) > 1 else "/tmp/dem/gebco.png"
OUT = sys.argv[2] if len(sys.argv) > 2 else "/tmp/dem/relief_2048.png"
W = int(sys.argv[3]) if len(sys.argv) > 3 else 2048
H = W // 2

print(f"abrindo {SRC} ...")
src = Image.open(SRC).convert("L")
print(f"  original {src.size}, reduzindo para {W}x{H} ...")
work = src.resize((W, H), Image.LANCZOS)
z = np.asarray(work, dtype=np.float32)

# Hillshade estilo ESRI: luz no noroeste (azimute 315, altitude 45).
zf = 4.0  # exagero vertical
dy, dx = np.gradient(z)
# Normaliza o gradiente pela resolução (referência 2048) pra a intensidade do
# relevo não mudar quando troca o tamanho da textura.
gscale = W / 2048.0
zenith = np.deg2rad(90.0 - 45.0)
azimuth = np.deg2rad(360.0 - 315.0 + 90.0)
slope = np.arctan(zf * np.hypot(dx, dy) * gscale / 8.0)
aspect = np.arctan2(dy, -dx)
hs = (np.cos(zenith) * np.cos(slope)
      + np.sin(zenith) * np.sin(slope) * np.cos(azimuth - aspect))
hs = np.clip(hs, 0.0, 1.0)

# Centra no neutro: plano (~media) vira 128, encostas claras/sombras desviam.
mean = float(hs.mean())
rel = np.clip(0.5 + (hs - mean) * 1.5, 0.0, 1.0)
out = (rel * 255.0).astype(np.uint8)

Image.fromarray(out, "L").save(OUT, optimize=True)
print(f"  salvo {OUT}  min={out.min()} max={out.max()} mean={out.mean():.1f}")
