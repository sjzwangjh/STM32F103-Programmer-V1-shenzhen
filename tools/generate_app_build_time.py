from datetime import datetime
from pathlib import Path


def main() -> None:
    project_root = Path(__file__).resolve().parent.parent
    output = project_root / "USER" / "app_build_time_generated.h"
    build_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    content = (
        "/* Generated at build start. Do not edit manually. */\n"
        "#ifndef APP_BUILD_TIME_GENERATED_H\n"
        "#define APP_BUILD_TIME_GENERATED_H\n"
        f'#define APP_BUILD_TIME_TEXT "{build_time}"\n'
        "#endif\n"
    )
    output.write_text(content, encoding="ascii", newline="\n")
    print(f"App build time: {build_time}")


if __name__ == "__main__":
    main()
