from django.db import models

# Create your models here.
from django.db import models

from django.utils import timezone


class VendingMachine(models.Model):
    MACHINE_STATUS = [
        ('online', 'Online'),
        ('offline', 'Offline'),
        ('maintenance', 'Maintenance'),
        ('error', 'Error')
    ]

    machine_id = models.CharField(max_length=50, unique=True)
    name = models.CharField(max_length=100)
    location = models.CharField(max_length=200)
    status = models.CharField(max_length=20, choices=MACHINE_STATUS, default='offline')
    last_maintenance = models.DateTimeField(null=True, blank=True)
    installation_date = models.DateTimeField(auto_now_add=True)
    
    def __str__(self):
        return f"{self.name} ({self.machine_id})"

class WaterQuality(models.Model):
    machine = models.ForeignKey(VendingMachine, on_delete=models.CASCADE, related_name='water_qualities')
    tds_level = models.FloatField(help_text="Total Dissolved Solids in ppm")
    ph_level = models.FloatField(help_text="pH level of water")
    water_level = models.FloatField(help_text="Water level in percentage")
    timestamp = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-timestamp']

    @property
    def latest_quality(self):
        return self.water_qualities.first()  # Karena sudah di-order by timestamp di Meta

    @property
    def total_sales_today(self):
        today = timezone.now().date()
        return self.sales.filter(timestamp__date=today).count()

class SalesRecord(models.Model):
    machine = models.ForeignKey(VendingMachine, on_delete=models.CASCADE, related_name='sales')
    volume = models.IntegerField(help_text="Volume in ml")
    price = models.DecimalField(max_digits=10, decimal_places=2)
    timestamp = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-timestamp']