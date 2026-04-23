from django.shortcuts import render, redirect
from django.contrib.auth import login
from django.contrib.auth.decorators import login_required
from django.conf import settings

import subprocess
import os
import shlex

from .forms import SimpleSignupForm, AnalysisJobForm
from .models import AnalysisJob


def home(request):
    return render(request, 'analyzer/home.html')
    

def signup(request):
    if request.method == 'POST':
        form = SimpleSignupForm(request.POST)
        if form.is_valid():
            user = form.save(commit=False)
            user.set_password(form.cleaned_data['password'])
            user.save()
            login(request, user)
            return redirect('home')
    else:
        form = SimpleSignupForm()

    return render(request, 'registration/signup.html', {'form': form})

def _compile_command(upload_path, compiled_path, paradigm):
    #return compiler command
    is_cpp = upload_path.endswith('.cpp')
    compiler = 'g++' if is_cpp else 'gcc'
    base = [compiler, '-O2', upload_path, '-lm', '-o', compiled_path]

    if paradigm == 'openmp':
        base.insert(2, '-fopenmp')
    elif paradigm == 'pthreads':
        base += ['-lpthread']
    elif paradigm == 'mpi':
        base[0] = 'mpicxx' if is_cpp else 'mpicc'
    return base

def _ompcheck_run_args(ompcheck_path, paradigm, threads, runs, csv_path, report_path, program_path, extra_args):
    #build the full command
    extra = shlex.split(extra_args) if extra_args.strip() else []
    return [
        str(ompcheck_path),
        '--threads', threads,
        '--runs', str(runs),
        '--csv', csv_path,
        '--plot',
        '--report', report_path,
        '--paradigm', paradigm,
        '--', program_path,
    ]   + extra

@login_required
def new_analysis(request):
    if request.method == 'POST':
        form = AnalysisJobForm(request.POST, request.FILES)
        if form.is_valid():
            job = form.save(commit=False)
            job.user = request.user
            job.original_filename = job.uploaded_file.name
            job.status = 'running'
            job.save()

            upload_path = str(job.uploaded_file.path)
            paradigm = job.paradigm
            #compile if c file
            if str(upload_path).endswith(('.c', '.cpp')):
                compiled_path = upload_path.rsplit('.', 1)[0]
                compile_cmd = _compile_command(upload_path, compiled_path, paradigm)

                compile_result = subprocess.run(
                    compile_cmd,
                    capture_output=True,
                    text=True
                )

                #if compile fails stop
                if compile_result.returncode != 0:
                    job.stderr_text = compile_result.stderr
                    job.status = 'failed'
                    job.save()
                    return redirect('job_detail', job_id=job.id)

                #use compiled binary instead
                upload_path = compiled_path
            output_dir = os.path.join(settings.MEDIA_ROOT, f'job_{job.id}')
            os.makedirs(output_dir, exist_ok=True)

            csv_path = os.path.join(output_dir, 'results.csv')
            report_path = os.path.join(output_dir, 'report.txt')

            try:
                project_root = settings.BASE_DIR.parent
                ompcheck_path = project_root / 'bin' / 'ompcheck'

                command = _ompcheck_run_args(
                    ompcheck_path = ompcheck_path,
                    paradigm = paradigm,
                    threads = job.threads,
                    runs = job.runs,
                    csv_path = csv_path,
                    report_path = report_path,
                    program_path = upload_path,
                    extra_args = job.extra_args,
                )

                project_root = settings.BASE_DIR.parent

                result = subprocess.run(
                    command,
                    capture_output=True,
                    text=True,
                    cwd=str(project_root)
                )

                job.stdout_text = result.stdout
                job.stderr_text = result.stderr

                job.csv_file.name = f'job_{job.id}/results.csv'
                job.report_file.name = f'job_{job.id}/report.txt'

                job.runtime_plot.name = f'job_{job.id}/results_runtime.png'
                job.speedup_plot.name = f'job_{job.id}/results_speedup.png'
                job.efficiency_plot.name = f'job_{job.id}/results_efficiency.png'

                job.status = 'done'

            except Exception as e:
                job.stderr_text = str(e)
                job.status = 'failed'

            job.save()

            return redirect('job_detail', job_id=job.id)
    else:
        form = AnalysisJobForm()

    return render(request, 'analyzer/new_analysis.html', {'form': form})


@login_required
def job_detail(request, job_id):
    job = AnalysisJob.objects.get(id=job_id, user=request.user)
    return render(request, 'analyzer/job_detail.html', {'job': job})