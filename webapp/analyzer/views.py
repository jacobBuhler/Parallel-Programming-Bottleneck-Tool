from django.shortcuts import render, redirect
from django.contrib.auth import login
from django.contrib.auth.decorators import login_required
from django.conf import settings

import subprocess
import os

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

            upload_path = job.uploaded_file.path
            #compile if c file
            if upload_path.endswith(".c"):
                compiled_path = upload_path.replace(".c", "")

                compile_cmd = [
                    "gcc",
                    "-O2",
                    "-fopenmp",
                    upload_path,
                    '-lm',
                    "-o",
                    compiled_path
                ]

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

                command = [
                    str(ompcheck_path),
                    "--threads", job.threads,
                    "--runs", str(job.runs),
                    "--csv", csv_path,
                    "--plot",
                    "--report", report_path,
                    "--",
                    upload_path,
                    job.extra_args
                ]

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