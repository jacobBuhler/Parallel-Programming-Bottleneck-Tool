from django.db import models
from django.contrib.auth.models import User


class Profile(models.Model):
    user = models.OneToOneField(User, on_delete=models.CASCADE)
    display_name = models.CharField(max_length=100, blank=True)
    bio = models.TextField(blank=True)

    def __str__(self):
        return self.display_name or self.user.username


class AnalysisJob(models.Model):
    STATUS_CHOICES = [
        ('pending', 'Pending'),
        ('running', 'Running'),
        ('done', 'Done'),
        ('failed', 'Failed'),
    ]
    PARADIGM_CHOICES = [
        ('openmp', 'OpenMP'),
        ('mpi', 'MPI'),
        ('pthreads', 'pthreads'),
    ]

    user = models.ForeignKey(User, on_delete=models.CASCADE)
    title = models.CharField(max_length=200, default="Untitled Run")

    uploaded_file = models.FileField(upload_to='uploads/')
    original_filename = models.CharField(max_length=255, blank=True)

    paradigm = models.CharField(max_length=20, choices=PARADIGM_CHOICES, default='openmp')
    threads = models.CharField(max_length=100)
    runs = models.PositiveIntegerField(default=5)
    extra_args = models.CharField(max_length=255, blank=True)

    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='pending')

    stdout_text = models.TextField(blank=True)
    stderr_text = models.TextField(blank=True)

    csv_file = models.FileField(upload_to='results/csv/', blank=True, null=True)
    report_file = models.FileField(upload_to='results/reports/', blank=True, null=True)
    runtime_plot = models.ImageField(upload_to='results/plots/', blank=True, null=True)
    speedup_plot = models.ImageField(upload_to='results/plots/', blank=True, null=True)
    efficiency_plot = models.ImageField(upload_to='results/plots/', blank=True, null=True)

    created_at = models.DateTimeField(auto_now_add=True)

    def __str__(self):
        return f"{self.title} ({self.user.username})"