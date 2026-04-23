from django import forms
from django.contrib.auth.models import User
from .models import AnalysisJob


class SimpleSignupForm(forms.ModelForm):
    password = forms.CharField(widget=forms.PasswordInput)
    password_confirm = forms.CharField(widget=forms.PasswordInput)

    class Meta:
        model = User
        fields = ['username']

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