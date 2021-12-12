#!/usr/bin/env python3
"""
Merge all tile entries and PNGs in a compositing tileset directory into
a tile_config.json and tilesheet .png file(s) ready for use in CDDA.

Examples:

    %(prog)s ../CDDA-Tilesets/gfx/Retrodays/
    %(prog)s --use-all ../CDDA-Tilesets/gfx/UltimateCataclysm/

By default, output is written back to the source directory. Pass an output
directory as the last argument to place output files there instead. The
output directory will be created if it does not already exist.
"""

import argparse
import json
import logging
import os
import subprocess
import sys

from logging.config import dictConfig
from pathlib import Path
from typing import Any, Optional, Tuple, Union

try:
    import pyvips
    Vips = pyvips
except ImportError:
    import gi
    gi.require_version('Vips', '8.0')  # NoQA
    from gi.repository import Vips


class LevelTrackingFilter(logging.Filter):
    """
    Logging handler that will remember the highest level that was called
    """
    def __init__(self):
        super().__init__()
        self.level = logging.NOTSET

    def filter(self, record):
        self.level = max(self.level, record.levelno)
        return True


LOGGING_CONFIG = {
    'formatters': {
        'standard': {'format': '%(levelname)s %(funcName)s: %(message)s'},
    },
    'handlers': {
        'default': {
            'level': 'NOTSET',  # will be set later
            'formatter': 'standard',
            'class': 'logging.StreamHandler',
        },
    },
    'loggers': {
        __name__: {
            'handlers': ['default'],
            'level': 'INFO',
        },
    },
    'disable_existing_loggers': False,
    'version': 1,
}

PROPERTIES_FILENAME = 'tileset.txt'

PNGSAVE_ARGS = {
    'compression': 9,
    'strip': True,
    'filter': 8,
}

FALLBACK = {
    "file": "fallback.png",
    "tiles": [],
    "ascii": [
        {"offset": 0, "bold": False, "color": "BLACK"},
        {"offset": 256, "bold": True, "color": "WHITE"},
        {"offset": 512, "bold": False, "color": "WHITE"},
        {"offset": 768, "bold": True, "color": "BLACK"},
        {"offset": 1024, "bold": False, "color": "RED"},
        {"offset": 1280, "bold": False, "color": "GREEN"},
        {"offset": 1536, "bold": False, "color": "BLUE"},
        {"offset": 1792, "bold": False, "color": "CYAN"},
        {"offset": 2048, "bold": False, "color": "MAGENTA"},
        {"offset": 2304, "bold": False, "color": "YELLOW"},
        {"offset": 2560, "bold": True, "color": "RED"},
        {"offset": 2816, "bold": True, "color": "GREEN"},
        {"offset": 3072, "bold": True, "color": "BLUE"},
        {"offset": 3328, "bold": True, "color": "CYAN"},
        {"offset": 3584, "bold": True, "color": "MAGENTA"},
        {"offset": 3840, "bold": True, "color": "YELLOW"}
    ]
}


log = logging.getLogger(__name__)


class FailFastHandler(logging.StreamHandler):
    def emit(self, record):
        sys.exit(1)


def write_to_json(
    pathname: str,
    data: Union[dict, list],
    format_json: bool = False,
) -> None:
    """
    Write data to a JSON file
    """
    kwargs = {
        'ensure_ascii': False,
    }
    if format_json:
        kwargs['indent'] = 2

    with open(pathname, 'w', encoding='utf-8') as file:
        json.dump(data, file, cls=ComposeEncoder, **kwargs)

    if not format_json:
        return

    json_formatter = Path('tools/format/json_formatter.cgi')
    if json_formatter.is_file():
        cmd = [json_formatter, pathname]
        subprocess.call(cmd)
    else:
        log.warning(
            '%s not found, Python built-in formatter was used.',
            json_formatter)


def list_or_first(iterable: list) -> Any:
    """
    Strip unneeded container list if there is only one value

    >>> list_or_first([1])
    1
    >>> list_or_first([1, 2])
    [1, 2]
    """
    return iterable[0] if len(iterable) == 1 else iterable


def read_properties(filepath: str) -> dict:
    """
    tileset.txt reader
    """
    with open(filepath, 'r', encoding='utf-8') as file:
        pairs = {}
        for line in file.readlines():
            line = line.strip()
            if line and not line.startswith('#'):
                key, value = line.split(':')
                pairs[key.strip()] = value.strip()
    return pairs


class ComposingException(Exception):
    """
    Base class for all composing exceptions
    """


class Tileset:
    """
    Referenced sprites memory and handling, tile entries conversion
    """
    def __init__(
        self,
        source_dir: Path,
        output_dir: Path,
        use_all: bool = False,
        obsolete_fillers: bool = False,
        palette_copies: bool = False,
        palette: bool = False,
        format_json: bool = False,
        only_json: bool = False,
    ) -> None:
        self.source_dir = source_dir
        self.output_dir = output_dir
        self.use_all = use_all
        self.obsolete_fillers = obsolete_fillers
        self.palette_copies = palette_copies
        self.palette = palette
        self.format_json = format_json
        self.only_json = only_json
        self.output_conf_file = None

        self.pngnum = 0
        self.unreferenced_pngnames = {
            'main': [],
            'filler': [],
        }

        self.pngname_indexes = {'null_image': 0}

        if not self.source_dir.is_dir() or \
                not os.access(self.source_dir, os.R_OK):
            raise ComposingException(
                f'Error: cannot open directory {self.source_dir}')

        self.processed_ids = []
        info_path = self.source_dir / 'tile_info.json'
        self.sprite_width = 16
        self.sprite_height = 16
        self.pixelscale = 1
        self.info = [{}]

        if not os.access(info_path, os.R_OK):
            raise ComposingException(f'Error: cannot open {info_path}')

        with open(info_path, 'r', encoding='utf-8') as file:
            self.info = json.load(file)
            self.sprite_width = self.info[0].get('width', self.sprite_width)
            self.sprite_height = self.info[0].get('height', self.sprite_height)
            self.pixelscale = self.info[0].get('pixelscale', self.pixelscale)

    def determine_conffile(self) -> str:
        """
        Read JSON value from tileset.txt
        """
        properties = {}

        for candidate_path in (self.source_dir, self.output_dir):
            properties_path = candidate_path / PROPERTIES_FILENAME
            if os.access(properties_path, os.R_OK):
                properties = read_properties(properties_path)
                if properties:
                    break

        if not properties:
            raise ComposingException(f'No valid {PROPERTIES_FILENAME} found')

        conf_filename = properties.get('JSON', None)

        if not conf_filename:
            raise ComposingException(
                f'No JSON key found in {PROPERTIES_FILENAME}')

        self.output_conf_file = conf_filename
        return self.output_conf_file

    def compose(self) -> None:
        """
        Convert a composing tileset into a package readable by the game
        """
        self.output_dir.mkdir(parents=True, exist_ok=True)
        tileset_confpath = self.output_dir / self.determine_conffile()
        typed_sheets = {
            'main': [],
            'filler': [],
            'fallback': [],
        }
        fallback_name = 'fallback.png'

        # loop through tilesheets and parse all configs in subdirectories,
        # create sheet images
        added_first_null = False
        for config in self.info[1:]:
            sheet = Tilesheet(self, config)

            if not added_first_null:
                # TODO: create a separate single-sprite 1x1 null first sheet
                sheet.sprites.append(sheet.null_image)
                sheet.first_index = 0
                added_first_null = True

            if sheet.is_filler:
                sheet_type = 'filler'
            elif sheet.is_fallback:
                sheet_type = 'fallback'
            else:
                sheet_type = 'main'

            log.info('parsing %s tilesheet %s', sheet_type, sheet.name)
            if sheet_type != 'fallback':
                sheet.walk_dirs()

                # TODO: generate JSON first
                # then create sheets if there are no errors

                # write output PNGs
                if not sheet.write_composite_png():
                    continue

            typed_sheets[sheet_type].append(sheet)

        # FIXME: drop unused fillers here
        '''
        drop_unused_fillers()
        '''

        # combine config data in the correct order
        sheet_configs = typed_sheets['main'] + typed_sheets['filler'] \
            + typed_sheets['fallback']

        # prepare "tiles-new", but remember max index of each sheet in keys
        tiles_new_dict = dict()

        def create_tile_entries_for_unused(
            unused: list,
            fillers: bool,
        ) -> None:
            # the list must be empty without use_all
            for unused_png in unused:
                if unused_png in self.processed_ids:
                    if not fillers:
                        log.warning(
                            '%(1)s sprite was not mentioned in any tile '
                            'entry but there is a tile entry for the %(1)s ID',
                            {'1': unused_png})
                    if fillers and self.obsolete_fillers:
                        log.warning(
                            'there is a tile entry for %s '
                            'in a non-filler sheet',
                            unused_png)
                    continue
                unused_num = self.pngname_indexes[unused_png]
                sheet_min_index = 0
                for sheet_max_index, sheet_data in tiles_new_dict.items():
                    if sheet_min_index < unused_num <= sheet_max_index:
                        sheet_data['tiles'].append({
                            'id': unused_png,
                            'fg': unused_num,
                        })
                        self.processed_ids.append(unused_png)
                        break
                    sheet_min_index = sheet_max_index

        main_finished = False

        for sheet in sheet_configs:
            if sheet.is_fallback:
                fallback_name = sheet.name
                continue
            if sheet.is_filler and not main_finished:
                create_tile_entries_for_unused(
                    self.handle_unreferenced_sprites('main'),
                    fillers=False
                )
                main_finished = True
            sheet_entries = []

            for tile_entry in sheet.tile_entries:
                # TODO: pop?
                converted_tile_entry = tile_entry.convert()
                if converted_tile_entry:
                    sheet_entries.append(converted_tile_entry)

            sheet_conf = {
                'file': sheet.name,
                '//': f'range {sheet.first_index} to {sheet.max_index}',
            }

            if not sheet.is_standard():
                sheet_conf['sprite_width'] = sheet.sprite_width
                sheet_conf['sprite_height'] = sheet.sprite_height
                sheet_conf['sprite_offset_x'] = sheet.offset_x
                sheet_conf['sprite_offset_y'] = sheet.offset_y

            sheet_conf['tiles'] = sheet_entries

            tiles_new_dict[sheet.max_index] = sheet_conf

        if not main_finished:
            create_tile_entries_for_unused(
                self.handle_unreferenced_sprites('main'),
                fillers=False,
            )

        create_tile_entries_for_unused(
            self.handle_unreferenced_sprites('filler'),
            fillers=True,
        )

        # finalize "tiles-new" config
        tiles_new = list(tiles_new_dict.values())

        FALLBACK['file'] = fallback_name
        tiles_new.append(FALLBACK)
        output_conf = {
            'tile_info': [{
                'pixelscale': self.pixelscale,
                'width': self.sprite_width,
                'height': self.sprite_height,
            }],
            'tiles-new': tiles_new
        }

        # save the config
        write_to_json(tileset_confpath, output_conf, self.format_json)

    def handle_unreferenced_sprites(
        self,
        sheet_type: str,
    ) -> list:
        """
        Either warn about unused sprites or return the list
        """
        if self.use_all:
            return self.unreferenced_pngnames[sheet_type]

        for pngname in self.unreferenced_pngnames[sheet_type]:
            if pngname in self.processed_ids:
                log.error(
                    '%(1)s.png not used when %(1)s ID '
                    'is mentioned in a tile entry',
                    {'1': pngname})

            else:
                log.warning(
                    'sprite filename %s was not used in any %s %s entries',
                    pngname, sheet_type, self.output_conf_file)
        return []


class Tilesheet:
    """
    Tilesheet reading and compositing
    """
    def __init__(
        self,
        tileset: Tileset,
        config: dict,  # {'name': {'sprite_width': 32, 'sprite_height': 32}}
    ) -> None:
        self.tileset = tileset

        self.name = next(iter(config))
        specs = config.get(self.name, {})

        self.sprite_width = specs.get(
            'sprite_width', tileset.sprite_width)
        self.sprite_height = specs.get(
            'sprite_height', tileset.sprite_height)
        self.offset_x = specs.get('sprite_offset_x', 0)
        self.offset_y = specs.get('sprite_offset_y', 0)

        self.sprites_across = specs.get('sprites_across', 16)
        self.exclude = specs.get('exclude', tuple())

        self.is_fallback = specs.get('fallback', False)
        self.is_filler = not self.is_fallback \
            and specs.get('filler', False)

        output_root = self.name.split('.png')[0]
        dir_name = \
            f'pngs_{output_root}_{self.sprite_width}x{self.sprite_height}'
        self.subdir_path = tileset.source_dir / dir_name

        self.output = tileset.output_dir / self.name

        self.tile_entries = []
        self.null_image = \
            Vips.Image.grey(self.sprite_width, self.sprite_height)
        self.sprites = []

        self.first_index = self.tileset.pngnum + 1

    @property
    def empty_spaces(self) -> int:
        """
        Number of unused slots in the end
        """
        value = self.sprites_across - \
            ((len(self.sprites) % self.sprites_across) or self.sprites_across)
        return value

    @property
    def max_index(self) -> int:
        """
        The index of the last sprite
        """
        return self.first_index + len(self.sprites) - 1 + self.empty_spaces

    def is_standard(self) -> bool:
        """
        Check whether output object needs a non-standard size or offset config
        """
        if self.offset_x or self.offset_y:
            return False
        if self.sprite_width != self.tileset.sprite_width:
            return False
        if self.sprite_height != self.tileset.sprite_height:
            return False
        return True

    def walk_dirs(self) -> None:
        """
        Find and process all JSON and PNG files within sheet directory
        """
        all_files = sorted(os.walk(self.subdir_path), key=lambda d: d[0])
        excluded_paths = [  # TODO: dict by parent dirs
            self.subdir_path / ignored_path for ignored_path in self.exclude
        ]

        for subdir_fpath, dirs, filenames in all_files:
            subdir_fpath = Path(subdir_fpath)
            if excluded_paths:
                # replace dirs in-place to prevent walking down excluded paths
                dirs[:] = [
                    d for d in dirs
                    if subdir_fpath / d not in excluded_paths
                ]

            for filename in sorted(filenames):
                filepath = subdir_fpath / filename

                if filepath.suffixes == ['.png']:
                    self.process_png(filepath)

                elif filepath.suffixes == ['.json']:
                    self.process_json(filepath)

    def process_png(
        self,
        filepath: Path,
    ) -> None:
        """
        Verify image root name is unique, load it and register
        """
        if filepath.stem in self.tileset.pngname_indexes:
            if not self.is_filler:
                log.error(
                    'duplicate root name %s: %s',
                    filepath.stem, filepath)

            if self.is_filler and self.tileset.obsolete_fillers:
                log.warning(
                    'root name %s is already present in a non-filler sheet: '
                    '%s',
                    filepath.stem, filepath)

            return

        if not self.tileset.only_json:
            self.sprites.append(self.load_image(filepath))
        else:
            self.sprites.append(None)

        self.tileset.pngnum += 1
        self.tileset.pngname_indexes[filepath.stem] = self.tileset.pngnum
        self.tileset.unreferenced_pngnames[
            'filler' if self.is_filler else 'main'].append(filepath.stem)

    def load_image(
        self,
        png_path: Union[str, Path],
    ) -> pyvips.Image:
        """
        Load and verify an image using pyvips
        """
        try:
            image = Vips.Image.pngload(str(png_path))
        except pyvips.error.Error as pyvips_error:
            raise ComposingException(
                f'Cannot load {png_path}: {pyvips_error.message}') from None
        except UnicodeDecodeError:
            raise ComposingException(
                f'Cannot load {png_path} with UnicodeDecodeError, '
                'please report your setup at '
                'https://github.com/libvips/pyvips/issues/80') from None
        if image.interpretation != 'srgb':
            image = image.colourspace('srgb')

        try:
            if not image.hasalpha():
                image = image.addalpha()
            if image.get_typeof('icc-profile-data') != 0:
                image = image.icc_transform('srgb')
        except Vips.Error as vips_error:
            log.error(
                '%s: %s',
                png_path, vips_error)

        if (image.width != self.sprite_width or
                image.height != self.sprite_height):
            log.error(
                '%s is %sx%s, but %s sheet sprites have to be %sx%s.',
                png_path, image.width, image.height, self.name,
                self.sprite_width, self.sprite_height)

        return image

    def process_json(
        self,
        filepath: Path,
    ) -> None:
        """
        Load and store tile entries from the file
        """
        with open(filepath, 'r', encoding='utf-8') as file:
            try:
                tile_entries = json.load(file)
            except Exception:
                log.error(
                    'error loading %s',
                    filepath)
                raise

            if not isinstance(tile_entries, list):
                tile_entries = [tile_entries]
            for input_entry in tile_entries:
                self.tile_entries.append(
                    TileEntry(self, input_entry, filepath))

    def write_composite_png(self) -> bool:
        """
        Compose and save tilesheet PNG if there are sprites to work with
        """
        if not self.sprites:
            return False

        # count empty spaces in the last row
        self.tileset.pngnum += self.empty_spaces

        if self.tileset.only_json:
            return True

        sheet_image = Vips.Image.arrayjoin(
            self.sprites, across=self.sprites_across)

        pngsave_args = PNGSAVE_ARGS.copy()

        if self.tileset.palette:
            pngsave_args['palette'] = True

        sheet_image.pngsave(str(self.output), **pngsave_args)

        if self.tileset.palette_copies and not self.tileset.palette:
            sheet_image.pngsave(
                str(self.output) + '8', palette=True, **pngsave_args)

        return True


class TileEntry:
    """
    Tile entry handling
    """
    def __init__(
        self,
        tilesheet: Tilesheet,
        data: dict,
        filepath: Union[str, Path],
    ) -> None:
        self.tilesheet = tilesheet
        self.data = data
        self.filepath = filepath

    def convert(
        self,
        entry: Union[dict, None] = None,
        prefix: str = '',
    ) -> Optional[dict]:
        """
        Recursively compile input into game-compatible objects in-place
        """
        if entry is None:
            entry = self.data

        entry_ids = entry.get('id')
        fg_layer = entry.get('fg')
        bg_layer = entry.get('bg')

        if not entry_ids or (not fg_layer and not bg_layer):
            log.warning(
                'skipping empty entry in %s%s',
                self.filepath,
                f' with IDs {prefix}{entry_ids} ' if entry_ids else '',
            )
            return None

        # make sure entry_ids is a list
        if entry_ids:
            if not isinstance(entry_ids, list):
                entry_ids = [entry_ids]

        # convert fg value
        if fg_layer:
            entry['fg'] = list_or_first(self.convert_entry_layer(fg_layer))
        else:
            # don't pop at the start because that affects order of the keys
            entry.pop('fg', None)

        # convert bg value
        if bg_layer:
            entry['bg'] = list_or_first(self.convert_entry_layer(bg_layer))
        else:
            # don't pop at the start because that affects order of the keys
            entry.pop('bg', None)

        # recursively convert additional_tiles value
        additional_entries = entry.get('additional_tiles', [])
        for additional_entry in additional_entries:
            # recursive part
            self.convert(additional_entry, f'{entry_ids[0]}_')

        # remember processed IDs and remove duplicates
        for entry_id in entry_ids:
            full_id = f'{prefix}{entry_id}'

            if full_id not in self.tilesheet.tileset.processed_ids:
                self.tilesheet.tileset.processed_ids.append(full_id)

            else:
                entry_ids.remove(entry_id)

                if self.tilesheet.is_filler:
                    if self.tilesheet.tileset.obsolete_fillers:
                        log.warning(
                            'skipping filler for %s from %s',
                            full_id, self.filepath)

                else:
                    log.error(
                        '%s encountered more than once, last time in %s',
                        full_id, self.filepath)

        # return converted entry if there are new IDs
        if entry_ids:
            entry['id'] = list_or_first(entry_ids)
            return entry

        return None

    def convert_entry_layer(
        self,
        entry_layer: Union[list, str],
    ) -> list:
        """
        Convert sprite names to sprite indexes in one fg or bg tile entry part
        """
        output = []

        if isinstance(entry_layer, list):
            # "fg": [ "f_fridge_S", "f_fridge_W", "f_fridge_N", "f_fridge_E" ]
            for layer_part in entry_layer:
                if isinstance(layer_part, dict):
                    # weighted random variations
                    variations, valid = self.convert_random_variations(
                        layer_part.get('sprite'))
                    if valid:
                        layer_part['sprite'] = \
                            variations[0] if len(variations) == 1 \
                            else variations
                        output.append(layer_part)
                else:
                    self.append_sprite(layer_part, output)
        else:
            # "bg": "t_grass"
            self.append_sprite(entry_layer, output)

        return output

    def convert_random_variations(
        self,
        sprite_names: Union[list, str],
    ) -> Tuple[list, bool]:
        """
        Convert list of random weighted variation objects
        """
        valid = False
        converted_variations = []

        if isinstance(sprite_names, list):
            # list of rotations
            converted_variations = []
            for sprite_name in sprite_names:
                valid |= self.append_sprite(
                    sprite_name, converted_variations)
        else:
            # single sprite
            valid = self.append_sprite(
                sprite_names, converted_variations)
        return converted_variations, valid

    def append_sprite(
        self,
        sprite_name: str,
        indexes_list: list,
    ) -> bool:
        """
        Get sprite index by sprite name and append it
        to the indexes_list in-place and return True or return False
        """
        if sprite_name:
            sprite = Sprite(sprite_name, self.tilesheet.tileset)
            if sprite.to_json() != 0:
                sheet_type = 'filler' if self.tilesheet.is_filler else 'main'
                try:
                    self.tilesheet.tileset\
                        .unreferenced_pngnames[sheet_type].remove(sprite_name)
                except ValueError:
                    pass

                indexes_list.append(sprite)
                return True

            log.error(
                '%(name)s.png file for %(name)s value from %(path)s '
                'was not found. It will not be added to %(target)s',
                {
                    'name': sprite_name,
                    'path': self.filepath,
                    'target': self.tilesheet.tileset.output_conf_file,
                }
            )

        return False


class Sprite:
    """
    Convert sprite IDs into indexes on writing JSON

    TODO: try moving the image into .image here
    """
    def __init__(
        self,
        name: str,
        tileset: Tileset,
    ):
        self.name = name
        self.tileset = tileset

    def to_json(self) -> int:
        """
        Convert sprite name into index
        """
        return self.tileset.pngname_indexes.get(self.name, 0)


class ComposeEncoder(json.JSONEncoder):
    """
    Custom encoder that calls to_json method
    """
    def default(self, o):
        """
        Call to_json method for all Sprite objects
        """
        if isinstance(o, Sprite):
            return o.to_json()
        return super().default(o)


def main() -> Union[int, ComposingException]:
    """
    Called when the script is executed directly
    """
    # read arguments and initialize objects
    arg_parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    arg_parser.add_argument(
        'source_dir', type=Path,
        help='Tileset source files directory path')
    arg_parser.add_argument(
        'output_dir', nargs='?', type=Path,
        help='Output directory path')
    arg_parser.add_argument(
        '--use-all', dest='use_all', action='store_true',
        help='Add unused images with id being their basename')
    arg_parser.add_argument(
        '--obsolete-fillers', dest='obsolete_fillers', action='store_true',
        help='Warn about obsoleted fillers')
    arg_parser.add_argument(
        '--palette-copies', dest='palette_copies', action='store_true',
        help='Produce copies of tilesheets quantized to 8bpp colormaps.')
    arg_parser.add_argument(
        '--palette', dest='palette', action='store_true',
        help='Quantize all tilesheets to 8bpp colormaps.')
    arg_parser.add_argument(
        '--format-json', dest='format_json', action='store_true',
        help='Use either CDDA formatter or Python json.tool '
        'to format the tile_config.json')
    arg_parser.add_argument(
        '--only-json', dest='only_json', action='store_true',
        help='Only output the tile_config.json')
    arg_parser.add_argument(
        '--fail-fast', dest='fail_fast', action='store_true',
        help='Stop immediately after an error has occured')
    arg_parser.add_argument(
        '--loglevel', dest='loglevel',
        choices=['INFO', 'WARNING', 'ERROR'],  # 'DEBUG', 'CRITICAL'
        default='WARNING',
        help="set verbosity level")

    args_dict = vars(arg_parser.parse_args())

    dictConfig(LOGGING_CONFIG)
    log.setLevel(getattr(logging, args_dict.get('loglevel')))
    log_tracker = LevelTrackingFilter()
    log.addFilter(log_tracker)

    if args_dict.get('fail_fast'):
        failfast_handler = FailFastHandler()
        failfast_handler.setLevel(logging.ERROR)
        log.addHandler(failfast_handler)

    # compose the tileset
    try:
        tileset_worker = Tileset(
            source_dir=Path(args_dict.get('source_dir')),
            output_dir=Path(
                args_dict.get('output_dir') or
                args_dict.get('source_dir')
            ),
            use_all=args_dict.get('use_all', False),
            obsolete_fillers=args_dict.get('obsolete_fillers', False),
            palette_copies=args_dict.get('palette_copies', False),
            palette=args_dict.get('palette', False),
            format_json=args_dict.get('format_json', False),
            only_json=args_dict.get('only_json', False),
        )
        tileset_worker.compose()
    except ComposingException as exception:
        return exception

    if log_tracker.level >= logging.ERROR:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
