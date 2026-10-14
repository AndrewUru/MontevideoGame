"""Validate reproducibility, raw immutability and optionally the actual C++ transform."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from process_public_spaces import ROOT, DEFAULT_SOURCE, DEFAULT_OUTPUT, build_derivative, digest, serialize


def raw_hashes():
    return {p.relative_to(ROOT).as_posix(): digest(p.read_bytes())
            for p in sorted((ROOT / 'Data/raw').rglob('*')) if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--processed', type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument('--unreal-editor', type=Path, help='Discovered UnrealEditor-Cmd.exe (optional C++ integration test)')
    args = parser.parse_args()
    before = raw_hashes()
    first = serialize(build_derivative(args.source))
    second = serialize(build_derivative(args.source))
    if first != second or first != args.processed.read_bytes():
        raise ValueError('Two runs and versioned derivative must be byte-identical')
    result = {'pipeline_runs': 2, 'byte_identical': True, 'derived_sha256': digest(first),
              'raw_files_checked': len(before), 'unreal_cpp_test': 'not requested'}
    if args.unreal_editor:
        executable = args.unreal_editor.resolve(strict=True)
        centroid = json.loads(first)['centroid_wgs84']
        report_dir = ROOT / 'Saved/M2_5/ProvenanceAutomation'
        # Reject stale reports instead of accidentally accepting an earlier success.
        report = report_dir / 'index.json'
        if report.exists():
            report.unlink()
        command = [str(executable), str(ROOT / 'MontevideoGame.uproject'),
                   '-unattended', '-nullrhi', '-nosound', '-nosplash',
                   '-ExecCmds=Automation RunTests Montevideo.DataProvenance.PlazaOrigin',
                   '-TestExit=Automation Test Queue Empty', f'-ReportExportPath={report_dir}',
                   f'-abslog={ROOT / "Saved/M2_5/provenance-tests.log"}',
                   f'-GeoTestLatitude={centroid["latitude"]:.17g}',
                   f'-GeoTestLongitude={centroid["longitude"]:.17g}']
        print('Running actual FGeoReference with validated centroid', file=sys.stderr, flush=True)
        console = ROOT / 'Saved/M2_5/provenance-console.log'
        console.parent.mkdir(parents=True, exist_ok=True)
        with console.open('wb') as stream:
            completed = subprocess.run(command, stdout=stream, stderr=subprocess.STDOUT, timeout=600)
        if completed.returncode != 0:
            raise ValueError(f'Editor exit {completed.returncode}; inspect {console}')
        report_data = json.loads(report.read_text(encoding='utf-8-sig'))
        if report_data['succeeded'] != 1 or report_data['failed'] or report_data['notRun'] or report_data['succeededWithWarnings']:
            raise ValueError('Expected exactly one successful C++ provenance test')
        result['unreal_cpp_test'] = {'exit_code': completed.returncode, 'succeeded': 1, 'tolerance_cm': 0.001,
                                    'synthetic_heights_m': [0, 123], 'command': command}
    if before != raw_hashes():
        raise ValueError('Data/raw changed during validation')
    result['raw_unchanged'] = True
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
