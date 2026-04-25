from django.urls import path
from . import views

urlpatterns = [
    path('', views.home, name='home'),
    path('signup/', views.signup, name='signup'),
    path('analysis/', views.job_list, name='job_list'),
    path('analysis/new/', views.new_analysis, name='new_analysis'),
    path('analysis/delete/', views.delete_jobs, name='delete_jobs'),
    path('analysis/<int:job_id>/', views.job_detail, name='job_detail'),
]