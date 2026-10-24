"""The manual for coding assistants: llms.txt and llms-full.txt, beside the built site.

llms.txt is an index of the manual's pages, in the order of its navigation, each with a line about
it; llms-full.txt is every page one after the other, for a tool to search. MkDocs runs this as a
hook (mkdocs.yml: hooks) once the site is built; by hand:

    python3 tools/docs/llms_txt.py <out_dir>

writes both into <out_dir>. Only the standard library is needed: the navigation is read from
mkdocs.yml, whose `nav:` is a list of `- Title: page.md` and `- Section:` lines.
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MKDOCS_FILE = ROOT / "mkdocs.yml"
INDEX_NAME = "llms.txt"
FULL_NAME = "llms-full.txt"
NAV_KEY = "nav:"
# "  - Title: page.md" (a page) or "  - Section:" (a section of the pages under it).
NAV_LINE = re.compile(r"^(?P<indent>\s*)- (?P<title>[^:]+):\s*(?P<page>\S*)\s*$")
SETTING_LINE = re.compile(r"^(?P<key>site_name|site_description|site_url|docs_dir):\s*(?P<value>.+)$")
SENTENCE_END = re.compile(r"(?<=[.!?])\s")
MARKDOWN_LINK = re.compile(r"\[([^\]]+)\]\([^)]*\)")
# A page's front matter (MkDocs takes it out of the page): "---", "description: ...", "---".
FRONT_MATTER = re.compile(r"\A---\n(?P<settings>.*?)\n---\n+", re.DOTALL)
QUOTE_ESCAPE = re.compile(r"\\(.)")
DESCRIPTION_LINE = re.compile(r"^description:\s*(?P<text>.+)$", re.MULTILINE)
PAGE_SUFFIX = ".md"
INDEX_PAGE = "index.md"
DEFAULT_DOCS_DIR = "docs"


@dataclass
class Page:
    title: str
    path: str  # inside the docs folder: "scripting/api.md"
    section: str


@dataclass
class Manual:
    name: str
    description: str
    url: str
    docs_dir: Path
    pages: list


def settings_of(lines):
    """The few top-level settings of mkdocs.yml this needs."""
    found = {}
    for line in lines:
        match = SETTING_LINE.match(line)
        if match:
            found[match["key"]] = match["value"].strip()
    return found


def nav_lines(lines):
    """The lines of mkdocs.yml's `nav:`, up to the next top-level setting."""
    start = lines.index(NAV_KEY) + 1
    taken = []
    for line in lines[start:]:
        if line and not line[0].isspace():
            break
        taken.append(line)
    return taken


def pages_of(lines):
    """The pages of the navigation, in its order, each with the section it is in ("" for a page at
    the top of the navigation, beside the sections)."""
    entries = [match for match in map(NAV_LINE.match, nav_lines(lines)) if match]
    top = min((len(entry["indent"]) for entry in entries), default=0)
    pages = []
    section = ""
    for entry in entries:
        at_top = len(entry["indent"]) == top
        if not entry["page"].endswith(PAGE_SUFFIX):
            section = entry["title"].strip()
            continue
        pages.append(Page(entry["title"].strip(), entry["page"], "" if at_top else section))
    return pages


def read_manual(mkdocs_file=MKDOCS_FILE):
    lines = mkdocs_file.read_text(encoding="utf-8").splitlines()
    settings = settings_of(lines)
    docs_dir = mkdocs_file.parent / settings.get("docs_dir", DEFAULT_DOCS_DIR)
    url = settings.get("site_url", "")
    return Manual(settings.get("site_name", ""), settings.get("site_description", ""), url, docs_dir,
                  pages_of(lines))


def page_url(manual, page):
    """Where the built site has a page: "scripting/api.md" -> "<site>/scripting/api/"."""
    if page.path == INDEX_PAGE:
        return manual.url
    return manual.url.rstrip("/") + "/" + page.path.removesuffix(PAGE_SUFFIX) + "/"


def first_sentence(markdown):
    """The first sentence of a page's first paragraph, its links reduced to their text."""
    for paragraph in markdown.split("\n\n"):
        text = " ".join(paragraph.split())
        if text and text[0].isalpha():
            text = MARKDOWN_LINK.sub(r"\1", text)
            return SENTENCE_END.split(text, maxsplit=1)[0]
    return ""


def page_source(manual, page):
    return (manual.docs_dir / page.path).read_text(encoding="utf-8")


def page_text(manual, page):
    """A page as it reads, without its front matter."""
    return FRONT_MATTER.sub("", page_source(manual, page), count=1)


def unquoted(value):
    """A YAML value in double quotes, as it reads ("a: b" -> a: b)."""
    if len(value) >= len('""') and value[0] == value[-1] == '"':
        return QUOTE_ESCAPE.sub(r"\1", value[1:-1])
    return value


def description_of(manual, page):
    """What a page's front matter says it is about; else its first sentence."""
    front = FRONT_MATTER.match(page_source(manual, page))
    found = DESCRIPTION_LINE.search(front["settings"]) if front else None
    return unquoted(found["text"].strip()) if found else first_sentence(page_text(manual, page))


def index_text(manual):
    lines = [f"# {manual.name}", "", f"> {manual.description}", ""]
    section = ""
    for page in manual.pages:
        if page.section != section:
            section = page.section
            lines += ["", f"## {section or 'More'}", ""]
        lines.append(f"- [{page.title}]({page_url(manual, page)}): {description_of(manual, page)}")
    return "\n".join(lines) + "\n"


def full_text(manual):
    parts = [f"# {manual.name}\n\n> {manual.description}\n"]
    for page in manual.pages:
        parts.append(f"\n---\n\nPage: {page_url(manual, page)}\n\n{page_text(manual, page)}")
    return "".join(parts)


def write_files(out_dir, mkdocs_file=MKDOCS_FILE):
    manual = read_manual(mkdocs_file)
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / INDEX_NAME).write_text(index_text(manual), encoding="utf-8")
    (out / FULL_NAME).write_text(full_text(manual), encoding="utf-8")


def on_post_build(config, **_):
    """MkDocs' hook: both files into the built site."""
    write_files(config["site_dir"], Path(config["config_file_path"]))


def main(arguments):
    if len(arguments) != 1:
        print(__doc__)
        return 2
    write_files(arguments[0])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
