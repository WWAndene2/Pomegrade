#!/usr/bin/env python3
"""The eden_ prefix on Eden's Android resources that clash with another module's (CLAUDE.md §4.2: 363 of them), for an
Eden update (31 October, the update to master 26a03d24; the session's resnames.py, othernames.py and rename.py, kept).

  eden_resource_prefix.py carry OLD_UPSTREAM PUNICA_EDEN OUT.json
      the names the previous update prefixed: in OLD_UPSTREAM's res (Eden as released) and found in PUNICA_EDEN's
      (the repository's eden/) only as eden_<name> (styles: Eden<name>)
  eden_resource_prefix.py clashes NEW_EDEN IN.json OUT.json
      IN.json plus the names of NEW_EDEN's res that another module of the repository also defines (same type)
  eden_resource_prefix.py apply NEW_EDEN NAMES.json
      renames them in NEW_EDEN: definitions (values, files) and references (@type/name, R.type.name, layouts'
      <Name>Binding classes)

Order for an update: carry (old release, repository) -> clashes (new tree) -> apply (new tree), then build. The roots
are Eden's top folders (holding src/android). Run from the repository's root (clashes reads the other modules there).
"""
import json
import os
import re
import sys

VALUE_TAGS = r'string|color|dimen|style|array|string-array|integer-array|plurals|integer|bool|attr|declare-styleable|item|fraction'
FILE_TYPES = {'layout', 'drawable', 'xml', 'mipmap', 'navigation', 'menu', 'anim', 'color', 'raw', 'font'}
RENAMED_VALUES = {'string': ['string'], 'dimen': ['dimen'], 'array': ['string-array', 'integer-array', 'array'],
                  'bool': ['bool'], 'color': ['color'], 'integer': ['integer'], 'plurals': ['plurals']}


def res_of(root):
    return os.path.join(root, 'src/android/app/src/main/res')


def scan(res, out):
    """every resource name defined under a res folder, by type"""
    for folder in os.listdir(res):
        path = os.path.join(res, folder)
        if not os.path.isdir(path):
            continue
        kind = folder.split('-')[0]
        for name in os.listdir(path):
            if kind != 'values':
                out.setdefault(kind, set()).add(name.split('.')[0])
                continue
            text = open(os.path.join(path, name), encoding='utf-8', errors='ignore').read()
            for m in re.finditer(r'<(%s)\b[^>]*\bname=["\']([^"\']+)["\']' % VALUE_TAGS, text):
                k = {'string-array': 'array', 'integer-array': 'array'}.get(m.group(1), m.group(1))
                if k == 'item':
                    t = re.search(r'type="([^"]+)"', m.group(0))
                    k = t.group(1) if t else 'item'
                out.setdefault(k, set()).add(m.group(2))
    return out


def carry(old, punica, out):
    a, b = scan(res_of(old), {}), scan(res_of(punica), {})
    names = {}
    for kind, ns in a.items():
        for n in ns:
            if n in b.get(kind, ()):
                continue
            if ('eden_' + n) in b.get(kind, ()) or (kind == 'style' and ('Eden' + n) in b.get(kind, ())):
                names.setdefault(kind, []).append(n)
    json.dump({k: sorted(v) for k, v in names.items()}, open(out, 'w'), indent=0)
    print({k: len(v) for k, v in names.items()}, sum(len(v) for v in names.values()))


def clashes(new, given, out):
    others = {}
    for path, _, _ in os.walk('.'):
        if any(x in path + '/' for x in ('/eden/', '/.git', '/build/', '/.cache', '/reference', '/tools/')):
            continue
        if path.endswith('/res') and '/src/main/' in path:
            scan(path, others)
    names = json.load(open(given))
    added = {}
    for kind, ns in scan(res_of(new), {}).items():
        for n in ns:
            if kind != 'item' and n in others.get(kind, ()) and n not in names.get(kind, []):
                added.setdefault(kind, []).append(n)
    print('new clashes:', {k: sorted(v) for k, v in added.items()})
    for kind, ns in added.items():
        names[kind] = sorted(set(names.get(kind, [])) | set(ns))
    json.dump(names, open(out, 'w'), indent=0)
    print('total', sum(len(v) for v in names.values()))


def apply(new, given):
    names = json.load(open(given))
    android, res = os.path.join(new, 'src/android'), res_of(new)
    camel = lambda n: ''.join(p[:1].upper() + p[1:] for p in n.split('_'))
    for folder in os.listdir(res):
        path = os.path.join(res, folder)
        if not os.path.isdir(path):
            continue
        kind = folder.split('-')[0]
        for f in os.listdir(path):
            fp = os.path.join(path, f)
            if kind == 'values':
                s = open(fp, encoding='utf-8').read()
                o = s
                for rt, tags in RENAMED_VALUES.items():
                    for n in names.get(rt, []):
                        for tag in tags:
                            s = re.sub(r'(<%s\b[^>]*\bname=["\'])%s(["\'])' % (re.escape(tag), re.escape(n)), r'\1eden_%s\2' % n, s)
                        s = re.sub(r'(<item\b[^>]*\bname=["\'])%s(["\'][^>]*type=["\']%s["\'])' % (re.escape(n), rt), r'\1eden_%s\2' % n, s)
                if s != o:
                    open(fp, 'w', encoding='utf-8').write(s)
            elif kind in FILE_TYPES and f.split('.')[0] in names.get(kind, []):
                os.rename(fp, os.path.join(path, 'eden_' + f))
    patterns = []
    for rt, ns in names.items():
        for n in ns:
            patterns.append((re.compile(r'(?<![\w.:])@%s/%s\b' % (rt, re.escape(n))), '@%s/eden_%s' % (rt, n)))
            patterns.append((re.compile(r'(?<!android\.)\bR\.%s\.%s\b' % (rt, re.escape(n))), 'R.%s.eden_%s' % (rt, n)))
            if rt == 'layout':
                patterns.append((re.compile(r'\b%sBinding\b' % camel(n)), 'Eden%sBinding' % camel(n)))
    for path, _, files in os.walk(android):
        if '/build' in path or '/.gradle' in path:
            continue
        for f in files:
            if not f.endswith(('.xml', '.kt', '.java')):
                continue
            fp = os.path.join(path, f)
            s = open(fp, encoding='utf-8').read()
            o = s
            if not any(k in s for k in ('@', 'R.', 'Binding')):
                continue
            for p, r in patterns:
                s = p.sub(r, s)
            if s != o:
                open(fp, 'w', encoding='utf-8').write(s)


def main(args):
    if args[:1] == ['carry'] and len(args) == 4:
        carry(*args[1:])
    elif args[:1] == ['clashes'] and len(args) == 4:
        clashes(*args[1:])
    elif args[:1] == ['apply'] and len(args) == 3:
        apply(*args[1:])
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
