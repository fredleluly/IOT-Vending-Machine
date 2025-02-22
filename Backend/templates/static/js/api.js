const BASE_URL = 'http://localhost:8000/api';

const api = {
    // Get all machines
    async getMachines() {
        try {
            const response = await fetch(`${BASE_URL}/machines/`);
            const data = await response.json();
            return data;
        } catch (error) {
            console.error('Error fetching machines:', error);
            throw error;
        }
    },

    // Get single machine detail
    async getMachineDetail(machineId) {
        try {
            const response = await fetch(`${BASE_URL}/machines/${machineId}/`);
            const data = await response.json();
            return data;
        } catch (error) {
            console.error('Error fetching machine detail:', error);
            throw error;
        }
    },

    // Record water quality
    async recordQuality(machineId, qualityData) {
        try {
            const response = await fetch(`${BASE_URL}/machines/${machineId}/record_quality/`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(qualityData)
            });
            const data = await response.json();
            return data;
        } catch (error) {
            console.error('Error recording quality:', error);
            throw error;
        }
    },

    // Record sale
    async recordSale(machineId, saleData) {
        try {
            const response = await fetch(`${BASE_URL}/machines/${machineId}/record_sale/`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(saleData)
            });
            const data = await response.json();
            return data;
        } catch (error) {
            console.error('Error recording sale:', error);
            throw error;
        }
    }
};