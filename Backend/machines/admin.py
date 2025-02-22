from django.contrib import admin

# Register your models here.
from django.contrib import admin
from .models import VendingMachine, WaterQuality, SalesRecord

# Tambahkan di admin.py
from django.contrib import admin
from .models import VendingMachine, WaterQuality, SalesRecord

class WaterQualityInline(admin.TabularInline):
    model = WaterQuality
    extra = 1

class VendingMachineAdmin(admin.ModelAdmin):
    inlines = [WaterQualityInline]
    list_display = ['name', 'machine_id', 'status', 'location']

    

admin.site.register(VendingMachine, VendingMachineAdmin)
admin.site.register(WaterQuality)
admin.site.register(SalesRecord)