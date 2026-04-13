from django.urls import path
from . import views

urlpatterns = [
    path('', views.home, name='home'),
    path('signup/', views.signup, name='signup'),
    path('analysis/new/', views.new_analysis, name='new_analysis'),
    path('analysis/<int:job_id>/', views.job_detail, name='job_detail'),
]