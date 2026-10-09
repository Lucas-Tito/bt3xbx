#!/usr/bin/env python3
"""Desenha o ícone 64x64 do título (arte própria, nada do jogo). Uso: make_icon.py SAIDA.png"""
import sys
from PIL import Image, ImageDraw, ImageFont

S = 4  # desenha em 256x256 e reduz, para borda suave
img = Image.new("RGBA", (64 * S, 64 * S), (18, 22, 40, 255))
d = ImageDraw.Draw(img)
d.ellipse((6 * S, 6 * S, 58 * S, 58 * S), fill=(245, 140, 30, 255), outline=(255, 200, 90, 255), width=2 * S)
font = ImageFont.load_default(size=22 * S)
d.text((32 * S, 33 * S), "BT3", font=font, anchor="mm", fill=(255, 255, 255, 255),
       stroke_width=2 * S, stroke_fill=(120, 40, 10, 255))
img.resize((64, 64), Image.LANCZOS).save(sys.argv[1], optimize=True)
