from django import forms
from django.contrib.auth.forms import AuthenticationForm
from django.contrib.auth.models import User
from .models import AnalysisJob


def _apply_bootstrap_classes(fields):
    #attach bootstrap widget class
    for field in fields.values():
        widget = field.widget
        existing = widget.attrs.get('class', '')

        if isinstance(widget, forms.Select):
            css = 'form-select'
        elif isinstance(widget, forms.CheckboxInput):
            css = 'form-check-input'
        elif isinstance(widget, forms.ClearableFileInput) or isinstance(widget, forms.FileInput):
            css = 'form-control'
        else:
            css = 'form-control'

        widget.attrs['class'] = (existing + ' ' + css).strip()


class SimpleSignupForm(forms.ModelForm):
    password = forms.CharField(widget=forms.PasswordInput)
    password_confirm = forms.CharField(widget=forms.PasswordInput)

    class Meta:
        model = User
        fields = ['username']

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _apply_bootstrap_classes(self.fields)

    def clean(self):
        cleaned_data = super().clean()
        password = cleaned_data.get("password")
        password_confirm = cleaned_data.get("password_confirm")

        if password != password_confirm:
            raise forms.ValidationError("Passwords do not match")

        return cleaned_data


class AnalysisJobForm(forms.ModelForm):
    class Meta:
        model = AnalysisJob
        fields = ['title', 'paradigm', 'uploaded_file', 'threads', 'runs', 'extra_args']

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _apply_bootstrap_classes(self.fields)


class BootstrapLoginForm(AuthenticationForm):
    #login form that wears bootstrap wrapper
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        _apply_bootstrap_classes(self.fields)