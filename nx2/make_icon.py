"""The app icon from a picture: an .ico for the builder and a .res the game's exe links in (nx2/build.py).

    python nx2/make_icon.py [picture.png, default assets/harpia.png] [out folder, default work]

Harpia's own badge is assets/harpia.png (transparent, 1024 px); assets/harpia.ico is this script's .ico of it, for the
builder. The .res goes to work/, where nx2/build.py links it into harpia.exe. Needs Pillow.
"""
import io
import os
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter

SIZES = (16, 24, 32, 48, 64, 128, 256)
HERE = os.path.dirname(os.path.abspath(__file__))
# 16 and 24 px: the whole badge is a blur there, so the head alone (beak and eye), as fractions of the picture, on a
# rounded square of the badge's own dark
SMALL, SMALL_CROP, SMALL_GROUND = 24, (0.146, 0.117, 0.772, 0.742), (14, 13, 20, 255)


def head(im, size):
    w, h = im.size
    c = SMALL_CROP
    crop = im.convert("RGBA").crop((round(c[0] * w), round(c[1] * h), round(c[2] * w), round(c[3] * h)))
    crop = crop.resize((size * 8, size * 8), Image.LANCZOS)
    mask = Image.new("L", crop.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, crop.width - 1, crop.height - 1), crop.width // 5, fill=255)
    out = Image.new("RGBA", crop.size, SMALL_GROUND)
    out.alpha_composite(crop)
    out.putalpha(mask)
    return out.resize((size, size), Image.LANCZOS)


def square(im, size):
    """fitted in a transparent square, not stretched"""
    if size <= SMALL:
        out = head(im, size)
    else:
        im = im.convert("RGBA")
        k = size / max(im.size)
        fit = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
        out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
        out.paste(fit, ((size - fit.width) // 2, (size - fit.height) // 2), fit)
    if size <= 48:   # (a downscaled painting goes soft: sharpen the small ones, colour only, the edge's alpha as it is)
        a = out.getchannel("A")
        out = out.filter(ImageFilter.UnsharpMask(radius=0.8 if size <= 24 else 1.0, percent=110, threshold=1))
        out.putalpha(a)
    return out


def png_of(im):
    b = io.BytesIO()
    im.save(b, "PNG", optimize=True)
    return b.getvalue()


def res_entry(kind, name, data, flags=0x1010):
    header = struct.pack("<II", len(data), 32) + struct.pack("<HHHH", 0xFFFF, kind, 0xFFFF, name)
    header += struct.pack("<IHHII", 0, flags, 0x0409, 0, 0)
    return header + data + b"\0" * (-len(data) & 3)


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "assets", "harpia.png")
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "work")
    im = Image.open(src)
    images = [(s, png_of(square(im, s))) for s in SIZES]

    head = struct.pack("<HHH", 0, 1, len(images))
    offset = len(head) + 16 * len(images)
    entries, body = b"", b""
    group = head
    for i, (s, data) in enumerate(images):
        b = s & 255
        entries += struct.pack("<BBBBHHII", b, b, 0, 0, 1, 32, len(data), offset + len(body))
        group += struct.pack("<BBBBHHIH", b, b, 0, 0, 1, 32, len(data), i + 1)
        body += data
    with open(os.path.join(out, "harpia.ico"), "wb") as f:
        f.write(head + entries + body)

    res = res_entry(0, 0, b"", 0)   # (a .res file starts with an empty entry)
    for i, (s, data) in enumerate(images):
        res += res_entry(3, i + 1, data)   # RT_ICON
    res += res_entry(14, 1, group, 0x1030)   # RT_GROUP_ICON: resource 1, what the window class loads
    with open(os.path.join(out, "harpia.res"), "wb") as f:
        f.write(res)
    print("make_icon: %d sizes, %d KB" % (len(images), len(res) >> 10))


if __name__ == "__main__":
    main()
