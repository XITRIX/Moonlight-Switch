"""
Copyright 2020 natinusala

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""
# Run with Python 3

import argparse
import json
import re
from jsonpointer import resolve_pointer, JsonPointerException
from pathlib import Path

# All locales supported by HOS
_SUPPORTED_LOCALES = [
    "ja",
    "en-US",
    "en-GB",
    "fr",
    "fr-CA",
    "de",
    "it",
    "es",
    "zh-CN",
    "zh-Hans",
    "zh-Hant",
    "zh-TW",
    "ko",
    "nl",
    "pt",
    "pt-BR",
    "ru",
    "es-419",
]

# The default locale used by brls
_DEFAULT_LOCALE = "en-US"

# All illegal chars in the string keys
# Mostly jsonpointer reserved chars
_ILLEGAL_KEYS_CHARS = [
    "/",
    "~",
    " ",
    "#",
    "$",
]

# All locales and their strings, filled by _check_locales
# {locale -> {key -> string}}
_LOCALES_CACHE = {}


def _folder_exists(path: Path, errors: list, warnings: list) -> tuple:
    """Checks that the i18n folder exists and is a folder"""
    if not path.exists():
        errors.append((1, f"Cannot continue with the checks: folder \"{path}\" doesn't exist"))
    elif not path.is_dir():
        errors.append((2, f"Cannot continue with the checks: file \"{path}\" is not a folder"))


def _ensure_default_locale(path: Path, errors: list, warnings: list) -> tuple:
    """Ensures that the default locale exists"""
    defaultlocalefile = path / f"{_DEFAULT_LOCALE}"
    if not defaultlocalefile.exists() or not defaultlocalefile.is_dir():
        warnings.append((6, f"Default locale {_DEFAULT_LOCALE} is missing from the i18n folder"))


def _check_locales(path: Path, errors: list, warnings: list) -> tuple:
    """
    Checks that the i18n only contains known locales
    and loads them for subsequent checks
    """
    for f in path.iterdir():
        # Load locale
        if f.is_dir():
            # Known locale
            if f.name not in _SUPPORTED_LOCALES:
                warnings.append((3, f"Unknown locale for folder \"{f.name}\""))
                continue
            #Load all JSON files inside
            for ff in f.iterdir():
                # Directory
                if ff.is_dir():
                    warnings.append((1, f"{f.name} folder contains stray folder \"{ff.name}\""))
                # Known format
                elif not ff.name.endswith(".json"):
                    warnings.append((2, f"{f.name} folder contains stray file \"{ff.name}\""))
                # Load it
                else:
                    with open(ff, "r") as jsonf:
                        _LOCALES_CACHE.setdefault(f.name, {})

                        try:
                            _LOCALES_CACHE[f.name][ff.name[:-5]] = json.loads(jsonf.read())
                        except json.JSONDecodeError as e:
                            errors.append((5, f"Cannot parse JSON file \"{f.name}/{ff.name}\": {e}"))
                            return  # don't bother continuing
        # File
        else:
            warnings.append((2, f"i18n folder contains stray file \"{f.name}\""))

    if _DEFAULT_LOCALE not in _LOCALES_CACHE:
        _LOCALES_CACHE[_DEFAULT_LOCALE] = {}

def _check_types(path: Path, errors: list, warnings: list) -> tuple:
    """Checks that locales only contain valid data"""
    def _check_node(breadcrumb: str, key: str, value: dict, locale: str):
        # Illegal chars in key
        for char in _ILLEGAL_KEYS_CHARS:
            if char in key:
                errors.append((6, f"String \"{breadcrumb}\" of {locale} locale contains illegal character \"{char}\" in its name"))

        # Dict
        if isinstance(value, dict):
            for nested_key in value:
                new_breadcrumb = f"{breadcrumb}/{nested_key}" if breadcrumb else nested_key
                _check_node(new_breadcrumb, nested_key, value[nested_key], locale)
        # Not strings
        elif not isinstance(value, str):
            errors.append((7, f"String \"{breadcrumb}\" of {locale} locale contains data \"{str(value)}\" of invalid type \"{type(value).__name__}\""))

    for locale in _LOCALES_CACHE:
        _check_node("", "", _LOCALES_CACHE[locale], locale)


def _check_untranslated_strings(path: Path, errors: list, warnings: list) -> tuple:
    """
    Ensure there are no untranslated strings
    """
    def _check_node(breadcrumb: str, value: dict):
        for nested_key in value:
            if breadcrumb:
                base = f"{breadcrumb}/{nested_key}"
            else:
                base = nested_key

            # Dict
            if isinstance(value[nested_key], dict):
                _check_node(base, value[nested_key])

            # Str
            else:
                for locale in _LOCALES_CACHE:
                    if locale == _DEFAULT_LOCALE:
                        continue

                    pointer = f"/{base}"
                    try:
                        resolve_pointer(_LOCALES_CACHE[locale], pointer)
                    except JsonPointerException:
                        warnings.append((4, f"Locale {locale} is missing string \"{base}\" (untranslated from {_DEFAULT_LOCALE})"))

    _check_node("", _LOCALES_CACHE[_DEFAULT_LOCALE])

def _check_unknown_translations(path: Path, errors: list, warnings: list) -> tuple:
    """Ensures all strings from all translations are in default locale"""
    default_locale = _LOCALES_CACHE[_DEFAULT_LOCALE]

    def _check_node(locale: str, breadcrumb: str, value: dict):
        # Dict
        if isinstance(value, dict):
            for nested_key in value:
                if breadcrumb:
                    base = f"{breadcrumb}/{nested_key}"
                else:
                    base = nested_key

                _check_node(locale, base, value[nested_key])

        # Str
        elif isinstance(value, str):
            pointer = f"/{breadcrumb}"

            try:
                resolve_pointer(default_locale, pointer)
            except JsonPointerException:
                warnings.append((5, f"String \"{breadcrumb}\" is translated in locale {locale} but is missing from default locale {_DEFAULT_LOCALE} (translation of unknown string)"))

    for locale in _LOCALES_CACHE:
        if locale == _DEFAULT_LOCALE:
            continue
        _check_node(locale, "", _LOCALES_CACHE[locale])

if __name__ == "__main__":
    # Arguments parsing
    parser = argparse.ArgumentParser(description="Check integrity of i18n strings")

    parser.add_argument(
        dest="path",
        action="store",
        help="The path to the i18n folder to check",
    )

    args = parser.parse_args()

    path = Path(args.path)

    print(f"Checking i18n folder {path}...\n")

    # Validation
    checks = [
        _folder_exists,
        _ensure_default_locale,
        _check_locales,
        _check_types,
        _check_untranslated_strings,
        _check_unknown_translations,
    ]

    errors = []
    warnings = []

    for check in checks:
        check(path, errors, warnings)

        if errors:
            break  # errors are fatal

    if warnings:
        print(f"{len(warnings)} warning(s):")

        for code, warning in warnings:
            print(f"     - W{code:02}: {warning}")

        print("\nWarnings are not fatal but should be fixed to avoid missing / broken translations in the app.")

    if errors:
        print(f"{len(errors)} error(s):")

        for code, error in errors:
            print(f"     - E{code:02}: {error}")

        print("\nPlease fix them and run the script again.")

    if not errors and not warnings:
        print("No errors or warnings detected, your i18n folder is good to go!")
