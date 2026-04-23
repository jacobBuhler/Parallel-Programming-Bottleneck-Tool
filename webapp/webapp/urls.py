from django.contrib import admin
from django.contrib.auth.views import LoginView
from django.urls import path, include
from django.conf import settings
from django.conf.urls.static import static

from analyzer.forms import BootstrapLoginForm

urlpatterns = [
    path('admin/', admin.site.urls),
    path(
        'accounts/login/',
        LoginView.as_view(authentication_form=BootstrapLoginForm),
        name='login',
    ),
    path('accounts/', include('django.contrib.auth.urls')),
    path('', include('analyzer.urls')),
]

if settings.DEBUG:
    urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)