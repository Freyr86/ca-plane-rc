"""Rebuild LakesideMeadow.jpg from the two 8K equirectangular source photos."""
import argparse
from pathlib import Path

import numpy as np
from PIL import Image


def compose(mountains_path, grass_path):
    with Image.open(mountains_path) as mountains, Image.open(grass_path) as meadow:
        if mountains.size != meadow.size or mountains.width != 2 * mountains.height:
            raise ValueError("Both panoramas must have matching 2:1 dimensions")
        width, height = mountains.size
        result = np.asarray(mountains.convert("RGB")).copy()
        grass = np.roll(np.asarray(meadow.convert("RGB")), int(width * .34), axis=1)

    columns = np.arange(width)
    angle = 2 * np.pi * columns / width
    # The material uses the same crest to confine the breeze to the grass.
    crest = .570 + .022*np.sin(2*angle + 1.1) + .013*np.cos(3*angle - 1.3) + .009*np.cos(5*angle)
    grass_top = .555 + .008*np.sin(angle)
    for row in range(int(.51 * height), height):
        latitude = row / height
        fraction = np.clip((latitude - crest) / (1 - crest), 0, 1)
        source_rows = ((grass_top + (1 - grass_top) * fraction) * height).astype(int)
        color = grass[np.minimum(source_rows, height - 1), columns] / 255.
        tips = .003*(color[:, 1] - .35) + .0006*np.sin(532*angle) + .0007*np.sin(213*angle)
        alpha = np.clip((latitude - crest + tips) / .0018 + .5, 0, 1)[:, None]
        result[row] = result[row]*(1 - alpha) + 255*color*[.94, .98, .98]*alpha
    return Image.fromarray(result)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("montorfano", type=Path)
    parser.add_argument("resting_place_2", type=Path)
    parser.add_argument("--output", type=Path, default=Path(__file__).with_name("LakesideMeadow.jpg"))
    args = parser.parse_args()
    compose(args.montorfano, args.resting_place_2).save(args.output, quality=96, subsampling=0)
