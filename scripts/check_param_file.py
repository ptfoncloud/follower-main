"""Parse a .param file the same way Mission Planner does and fail on anything
it would choke on.

Mission Planner's loader (ExtLibs/Utilities/ParamFile.cs, loadParamFile) only
skips a line if it STARTS with '#', then splits on space/comma/tab and parses
the 2nd token as a number. So an indented comment line like
"        # some note" becomes param "#" = "some" and the whole load fails with
"Invalid number on param # : some", and nothing gets loaded.

usage: python scripts/check_param_file.py params/follower.param
"""
import sys


def load_like_mission_planner(path):
    params = {}
    problems = []
    with open(path, encoding="utf-8") as f:
        for lineno, raw in enumerate(f, 1):
            line = raw.rstrip("\r\n")
            if line.startswith("#"):
                continue
            items = [t for t in line.replace(",", " ").replace("\t", " ").split(" ") if t]
            if len(items) < 2:
                continue
            name, value = items[0], items[1]
            try:
                float(value)
            except ValueError:
                problems.append(f"line {lineno}: Mission Planner would fail with "
                                f"'Invalid number on param {name} : {value}'")
                continue
            if name in params:
                problems.append(f"line {lineno}: {name} set twice (line {params[name]} too)")
            params[name] = lineno
    return params, problems


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "params/follower.param"
    params, problems = load_like_mission_planner(path)
    for p in problems:
        print(p)
    if problems:
        sys.exit(1)
    print(f"{path}: {len(params)} params, loads fine in Mission Planner")


if __name__ == "__main__":
    main()
