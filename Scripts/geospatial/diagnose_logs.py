"""Extract startup error windows and named smoke-test outcomes without filtering logs."""
import argparse
import hashlib
import json
from pathlib import Path
import re


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, default=Path('Saved/M2_5'))
    parser.add_argument('--output', type=Path, default=Path('Saved/M2_5/diagnostic.json'))
    args = parser.parse_args()
    result = {}
    for name in ('startup-no-tests.log', 'geospatial-tests.log', 'engine-control.log', 'engine-control-en.log'):
        data = (args.directory / name).read_bytes()
        lines = data.decode('utf-8', errors='replace').splitlines()
        hits = [i for i, line in enumerate(lines) if 'Condition failed' in line]
        window = set()
        for i in hits:
            window.update(range(max(0, i - 4), min(len(lines), i + 5)))
        result[name] = {
            'sha256': hashlib.sha256(data).hexdigest(), 'condition_failed_count': len(hits),
            'diagnostic_window': [{'line': i + 1, 'text': lines[i]} for i in sorted(window)],
            'named_smoke_outcomes': [line for line in lines if re.search(r'LogAutomationTest: (FUnifiedErrorTest_CreateErrorMessage\w*|FStructuredLogFormatTest):', line)],
            'error_ensure_assertion': [{'line': i + 1, 'text': line} for i, line in enumerate(lines)
                                       if re.search(r'Error:|Ensure condition failed|Assertion failed', line)],
        }
    suite = json.loads((args.directory / 'Automation/index.json').read_text(encoding='utf-8-sig'))
    result['m2_suite'] = {key: suite[key] for key in ('succeeded', 'succeededWithWarnings', 'failed', 'notRun')}
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({name: record.get('condition_failed_count', record) for name, record in result.items()}))


if __name__ == '__main__':
    main()
