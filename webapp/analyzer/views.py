from django.shortcuts import render, redirect
from django.contrib.auth import login
from django.contrib.auth.decorators import login_required
from django.conf import settings

import subprocess
import shutil
import os
import shlex
import platform

from .forms import SimpleSignupForm, AnalysisJobForm
from .models import AnalysisJob


def home(request):
    recent_jobs = []
    if request.user.is_authenticated:
        recent_jobs = AnalysisJob.objects.filter(user=request.user).order_by('-created_at')[:5]
    return render(request, 'analyzer/home.html', {'recent_jobs': recent_jobs})

@login_required
def job_list(request):
    jobs = AnalysisJob.objects.filter(user=request.user).order_by('-created_at')
    return render(request, 'analyzer/job_list.html', {'jobs': jobs})

def _delete_job_files(job):
    if job.uploaded_file:
        try:
            job.uploaded_file.delete(save=False)
        except Exception:
            pass
 
    #per job output dir
    output_dir = os.path.join(settings.MEDIA_ROOT, f'job_{job.id}')
    if os.path.isdir(output_dir):
        shutil.rmtree(output_dir, ignore_errors=True)
 
 
@login_required
def delete_jobs(request):
    #delete runs 1 or more
    if request.method != 'POST':
        return redirect('job_list')
 
    job_ids = request.POST.getlist('job_ids')
    if job_ids:
        jobs = AnalysisJob.objects.filter(user=request.user, id__in=job_ids)
        for job in jobs:
            _delete_job_files(job)
        jobs.delete()
 
    return redirect('job_list')


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


def _openmp_flags():
    if platform.system() == 'Darwin':
        try:
            libomp = subprocess.run(
                ['brew', '--prefix', 'libomp'],
                capture_output=True, text=True, check=True
            ).stdout.strip()
        except (subprocess.CalledProcessError, FileNotFoundError):
            libomp = '/opt/homebrew/opt/libomp'  # Apple Silicon default
        return (
            ['-Xpreprocessor', '-fopenmp', f'-I{libomp}/include'],
            [f'-L{libomp}/lib', '-lomp'],
        )
    return (['-fopenmp'], ['-fopenmp'])


def _compile_command(upload_path, compiled_path, paradigm):
    #return compiler command
    is_cpp = upload_path.endswith('.cpp')
    compiler = 'g++' if is_cpp else 'gcc'

    cmd = [compiler, '-O2']

    if paradigm == 'openmp':
        omp_cflags, omp_ldflags = _openmp_flags()
        cmd += omp_cflags
        cmd += [upload_path, '-lm']
        cmd += omp_ldflags
    elif paradigm == 'pthreads':
        cmd += [upload_path, '-lm', '-lpthread']
    elif paradigm == 'mpi':
        cmd[0] = 'mpicxx' if is_cpp else 'mpicc'
        cmd += [upload_path, '-lm']
    else:
        cmd += [upload_path, '-lm']

    cmd += ['-o', compiled_path]
    return cmd


def _paracheck_run_args(paracheck_path, paradigm, threads, runs, csv_path, report_path, program_path, extra_args):
    #build the full command
    extra = shlex.split(extra_args) if extra_args.strip() else []
    return [
        str(paracheck_path),
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
                paracheck_path = project_root / 'bin' / 'paracheck'

                command = _paracheck_run_args(
                    paracheck_path = paracheck_path,
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
                proj_runtime = os.path.join(output_dir, 'results_projected_runtime.png')
                proj_speedup = os.path.join(output_dir, 'results_projected_speedup.png')
                proj_efficiency = os.path.join(output_dir, 'results_projected_efficiency.png')
                if os.path.isfile(proj_runtime):
                    job.projected_runtime_plot.name = f'job_{job.id}/results_projected_runtime.png'

                if os.path.isfile(proj_speedup):
                    job.projected_speedup_plot.name = f'job_{job.id}/results_projected_speedup.png'

                if os.path.isfile(proj_efficiency):
                    job.projected_efficiency_plot.name = f'job_{job.id}/results_projected_efficiency.png'

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
