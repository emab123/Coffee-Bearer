/*
==================================================
ADMIN.JS - SCRIPT ESPECÍFICO PARA PÁGINAS ADMIN
Funcionalidades específicas para administradores
==================================================
*/

// Variáveis específicas do admin
let adminData = {
    stats: {},
    users: [],
    logs: [],
    settings: {}
};

let chartInstances = {};
let autoRefreshEnabled = true;

// ============== INICIALIZAÇÃO ESPECÍFICA DO ADMIN ==============

document.addEventListener('DOMContentLoaded', function() {
    if (window.location.pathname.includes('/admin/')) {
        initializeAdminFeatures();
    }
});

function initializeAdminFeatures() {
    console.log('Inicializando funcionalidades de admin...');
    
    // Verificar se é admin
    if (currentUser && currentUser.role !== 'Admin') {
        showAlert('Acesso negado - apenas administradores', 'error');
        window.location.href = '/user/dashboard';
        return;
    }
    
    // Configurar handlers específicos do admin
    setupAdminEventHandlers();
    
    // Inicializar funcionalidades baseadas na página atual
    const currentPage = getCurrentAdminPage();
    initializePageSpecificFeatures(currentPage);
}

function getCurrentAdminPage() {
    const path = window.location.pathname;
    
    if (path.includes('/admin/dashboard')) return 'dashboard';
    if (path.includes('/admin/users')) return 'users';
    if (path.includes('/admin/settings')) return 'settings';
    if (path.includes('/admin/logs')) return 'logs';
    if (path.includes('/admin/stats')) return 'stats';
    
    return 'dashboard';
}

function initializePageSpecificFeatures(page) {
    switch (page) {
        case 'dashboard':
            initializeDashboard();
            break;
        case 'users':
            initializeUsersPage();
            break;
        case 'settings':
            initializeSettingsPage();
            break;
        case 'logs':
            initializeLogsPage();
            break;
        case 'stats':
            initializeStatsPage();
            break;
    }
}

// ============== DASHBOARD ==============

function initializeDashboard() {
    loadDashboardData();
    startDashboardAutoRefresh();
}

async function loadDashboardData() {
    try {
        showLoading(true);
        
        // Carregar dados em paralelo
        const [statusData, usersData, logsData] = await Promise.all([
            apiRequest('/api/status'),
            apiRequest('/api/users'),
            apiRequest('/api/logs?limit=5')
        ]);
        
        if (statusData) {
            adminData.stats = statusData;
            updateDashboardStats(statusData);
        }
        
        if (usersData) {
            adminData.users = usersData.users || [];
            updateActiveUsersDisplay();
        }
        
        if (logsData) {
            adminData.logs = logsData.logs || [];
            updateRecentActivity();
        }
        
    } catch (error) {
        console.error('Erro ao carregar dados do dashboard:', error);
        showAlert('Erro ao carregar dados do dashboard', 'error');
    } finally {
        showLoading(false);
    }
}

function updateDashboardStats(data) {
    const statsGrid = document.getElementById('statsGrid');
    if (!statsGrid) return;
    
    statsGrid.innerHTML = `
        <div class="stat-card">
            <span class="stat-number">${data.users?.total || 0}</span>
            <span class="stat-label">Usuários Cadastrados</span>
        </div>
        <div class="stat-card">
            <span class="stat-number">${data.coffee?.totalServed || 0}</span>
            <span class="stat-label">Cafés Servidos</span>
        </div>
        <div class="stat-card">
            <span class="stat-number">${data.coffee?.remaining || 0}</span>
            <span class="stat-label">Cafés Restantes</span>
        </div>
        <div class="stat-card">
            <span class="stat-number">${data.users?.activeToday || 0}</span>
            <span class="stat-label">Usuários Ativos Hoje</span>
        </div>
        <div class="stat-card">
            <span class="stat-number">${data.auth?.activeSessions || 0}</span>
            <span class="stat-label">Sessões Ativas</span>
        </div>
        <div class="stat-card">
            <span class="stat-number">${formatUptime(data.system?.uptime || 0)}</span>
            <span class="stat-label">Tempo Ativo</span>
        </div>
    `;
    
    // Atualizar status do sistema
    updateSystemStatusIndicators(data);
}

function updateSystemStatusIndicators(data) {
    const indicators = {
        system: { icon: '#systemStatusIcon', text: '#systemStatusText' },
        coffee: { icon: '#coffeeStatusIcon', text: '#coffeeStatusText' },
        wifi: { icon: '#wifiStatusIcon', text: '#wifiStatusText' },
        rfid: { icon: '#rfidStatusIcon', text: '#rfidStatusText' }
    };
    
    // Status do sistema
    if (indicators.system.icon && indicators.system.text) {
        const systemIcon = document.querySelector(indicators.system.icon);
        const systemText = document.querySelector(indicators.system.text);
        
        if (systemIcon && systemText) {
            if (data.coffee?.isBusy) {
                systemIcon.textContent = '🟡';
                systemText.textContent = 'Ocupado';
            } else {
                systemIcon.textContent = '🟢';
                systemText.textContent = 'Online';
            }
        }
    }
    
    // Status do café
    if (indicators.coffee.icon && indicators.coffee.text) {
        const coffeeIcon = document.querySelector(indicators.coffee.icon);
        const coffeeText = document.querySelector(indicators.coffee.text);
        
        if (coffeeIcon && coffeeText) {
            if (data.coffee?.remaining <= 0) {
                coffeeIcon.textContent = '🔴';
                coffeeText.textContent = 'Vazio';
            } else if (data.coffee?.isBusy) {
                coffeeIcon.textContent = '🟡';
                coffeeText.textContent = 'Servindo';
            } else {
                coffeeIcon.textContent = '🟢';
                coffeeText.textContent = 'Pronto';
            }
        }
    }
    
    // Status WiFi
    if (indicators.wifi.icon && indicators.wifi.text) {
        const wifiIcon = document.querySelector(indicators.wifi.icon);
        const wifiText = document.querySelector(indicators.wifi.text);
        
        if (wifiIcon && wifiText) {
            if (data.system?.wifiConnected) {
                wifiIcon.textContent = '🟢';
                wifiText.textContent = 'Conectado';
            } else {
                wifiIcon.textContent = '🔴';
                wifiText.textContent = 'Desconectado';
            }
        }
    }
}

function updateActiveUsersDisplay() {
    const activeUsersGrid = document.getElementById('activeUsersGrid');
    const activeUsersCount = document.getElementById('activeUsersCount');
    
    if (!activeUsersGrid || !adminData.users) return;
    
    const activeUsers = adminData.users.filter(user => 
        user.isActive && user.credits > 0 && isActiveToday(user.lastUsed)
    );
    
    if (activeUsersCount) {
        activeUsersCount.textContent = activeUsers.length;
    }
    
    if (activeUsers.length === 0) {
        activeUsersGrid.innerHTML = '<p class="text-center">Nenhum usuário ativo hoje</p>';
        return;
    }
    
    activeUsersGrid.innerHTML = activeUsers.slice(0, 6).map(user => `
        <div class="user-card">
            <div class="user-avatar">${user.name.charAt(0).toUpperCase()}</div>
            <div class="user-info">
                <div class="user-name">${escapeHtml(user.name)}</div>
                <div class="user-credits">${user.credits} créditos</div>
            </div>
        </div>
    `).join('');
}

function updateRecentActivity() {
    const activityList = document.getElementById('activityList');
    if (!activityList || !adminData.logs) return;
    
    if (adminData.logs.length === 0) {
        activityList.innerHTML = '<div class="activity-item">Nenhuma atividade recente</div>';
        return;
    }
    
    activityList.innerHTML = adminData.logs.map(log => `
        <div class="activity-item">
            <div class="activity-icon">${getActivityIcon(log)}</div>
            <div class="activity-content">
                <div class="activity-text">${escapeHtml(log.message || log)}</div>
                <div class="activity-time">${formatLogTime(log)}</div>
            </div>
        </div>
    `).join('');
}

function startDashboardAutoRefresh() {
    if (autoRefreshEnabled) {
        autoRefreshInterval = setInterval(() => {
            if (!document.hidden && autoRefreshEnabled) {
                loadDashboardData();
            }
        }, 30000); // 30 segundos
    }
}

// ============== CONTROLES RÁPIDOS ==============

async function serveCoffee() {
    try {
        showLoading(true);
        
        const response = await apiRequest('/api/serve-coffee', {
            method: 'POST'
        });
        
        if (response && response.success) {
            showAlert('Café servido manualmente!', 'success');
            loadDashboardData(); // Atualizar dados
        } else {
            showAlert(response?.message || 'Erro ao servir café', 'error');
        }
    } catch (error) {
        console.error('Erro ao servir café:', error);
        showAlert('Erro de conexão ao servir café', 'error');
    } finally {
        showLoading(false);
    }
}

async function refillCoffee() {
    showConfirmDialog(
        'Reabastecer Garrafa',
        'Isso irá restaurar a contagem de cafés para o máximo. Confirmar?',
        async () => {
            try {
                showLoading(true);
                
                const response = await apiRequest('/api/refill-coffee', {
                    method: 'POST'
                });
                
                if (response && response.success) {
                    showAlert('Garrafa reabastecida!', 'success');
                    loadDashboardData();
                } else {
                    showAlert(response?.message || 'Erro ao reabastecer', 'error');
                }
            } catch (error) {
                console.error('Erro ao reabastecer:', error);
                showAlert('Erro de conexão ao reabastecer', 'error');
            } finally {
                showLoading(false);
            }
        }
    );
}

async function resetSystem() {
    showConfirmDialog(
        'Reiniciar Sistema',
        'O sistema será reiniciado. Esta ação pode levar alguns minutos. Confirmar?',
        async () => {
            try {
                showLoading(true);
                
                const response = await apiRequest('/api/system-reset', {
                    method: 'POST'
                });
                
                showAlert('Sistema reiniciando... Aguarde alguns instantes.', 'info');
                
                // Aguardar e recarregar página
                setTimeout(() => {
                    location.reload();
                }, 15000);
                
            } catch (error) {
                console.error('Erro ao reiniciar sistema:', error);
                showAlert('Erro ao reiniciar sistema', 'error');
                showLoading(false);
            }
        }
    );
}

async function exportData() {
    try {
        showLoading(true);
        
        const response = await fetch('/api/backup');
        
        if (response.ok) {
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `cafeteira_backup_${new Date().toISOString().split('T')[0]}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            showAlert('Backup exportado com sucesso!', 'success');
        } else {
            showAlert('Erro ao exportar dados', 'error');
        }
    } catch (error) {
        console.error('Erro no backup:', error);
        showAlert('Erro de conexão no backup', 'error');
    } finally {
        showLoading(false);
    }
}

// ============== AUTO-REFRESH ==============

function toggleAutoRefresh() {
    autoRefreshEnabled = !autoRefreshEnabled;
    
    const button = document.getElementById('autoRefreshText');
    if (button) {
        button.textContent = autoRefreshEnabled ? '⏸️ Pausar' : '▶️ Retomar';
    }
    
    if (autoRefreshEnabled) {
        startDashboardAutoRefresh();
        showAlert('Atualização automática ativada', 'info');
    } else {
        if (autoRefreshInterval) {
            clearInterval(autoRefreshInterval);
            autoRefreshInterval = null;
        }
        showAlert('Atualização automática pausada', 'info');
    }
}

function refreshActivity() {
    loadDashboardData();
}

// ============== LOGS PAGE ==============

function initializeLogsPage() {
    loadLogs();
    setupLogsAutoRefresh();
}

async function loadLogs(limit = 50) {
    try {
        showLoading(true);
        
        const response = await apiRequest(`/api/logs?limit=${limit}`);
        
        if (response && response.logs) {
            adminData.logs = response.logs;
            updateLogsDisplay();
        }
    } catch (error) {
        console.error('Erro ao carregar logs:', error);
        showAlert('Erro ao carregar logs', 'error');
    } finally {
        showLoading(false);
    }
}

function updateLogsDisplay() {
    const logsContainer = document.getElementById('logsContainer');
    if (!logsContainer) return;
    
    if (!adminData.logs || adminData.logs.length === 0) {
        logsContainer.innerHTML = '<p class="text-center">Nenhum log encontrado</p>';
        return;
    }
    
    logsContainer.innerHTML = adminData.logs.map(log => `
        <div class="log-entry log-${getLogLevel(log)}">
            <div class="log-time">${formatLogTime(log)}</div>
            <div class="log-level">${getLogLevel(log).toUpperCase()}</div>
            <div class="log-message">${escapeHtml(log.message || log)}</div>
            ${log.details ? `<div class="log-details">${escapeHtml(log.details)}</div>` : ''}
        </div>
    `).join('');
}

function setupLogsAutoRefresh() {
    if (autoRefreshEnabled) {
        setInterval(() => {
            if (!document.hidden) {
                loadLogs();
            }
        }, 10000); // 10 segundos para logs
    }
}

function clearLogs() {
    showConfirmDialog(
        'Limpar Logs',
        'Isso irá apagar todos os logs do sistema. Esta ação não pode ser desfeita. Confirmar?',
        async () => {
            try {
                showLoading(true);
                
                const response = await apiRequest('/api/logs', {
                    method: 'DELETE'
                });
                
                if (response && response.success) {
                    showAlert('Logs limpos com sucesso!', 'success');
                    adminData.logs = [];
                    updateLogsDisplay();
                } else {
                    showAlert('Erro ao limpar logs', 'error');
                }
            } catch (error) {
                console.error('Erro ao limpar logs:', error);
                showAlert('Erro de conexão ao limpar logs', 'error');
            } finally {
                showLoading(false);
            }
        }
    );
}

// ============== STATS PAGE ==============

function initializeStatsPage() {
    loadStatsData();
    initializeCharts();
}

async function loadStatsData() {
    try {
        showLoading(true);
        
        const response = await apiRequest('/api/stats');
        
        if (response) {
            updateStatsCharts(response);
        }
    } catch (error) {
        console.error('Erro ao carregar estatísticas:', error);
        showAlert('Erro ao carregar estatísticas', 'error');
    } finally {
        showLoading(false);
    }
}

function initializeCharts() {
    // Inicializar Chart.js se disponível
    if (typeof Chart !== 'undefined') {
        initializeConsumptionChart();
        initializeUsersChart();
    }
}

function initializeConsumptionChart() {
    const canvas = document.getElementById('consumptionChart');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    
    chartInstances.consumption = new Chart(ctx, {
        type: 'line',
        data: {
            labels: getLast7Days(),
            datasets: [{
                label: 'Cafés Servidos',
                data: [0, 0, 0, 0, 0, 0, 0], // Será atualizado com dados reais
                borderColor: 'rgb(75, 192, 192)',
                backgroundColor: 'rgba(75, 192, 192, 0.2)',
                tension: 0.1
            }]
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: true,
                    text: 'Consumo de Café - Últimos 7 Dias'
                }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    ticks: {
                        stepSize: 1
                    }
                }
            }
        }
    });
}

function initializeUsersChart() {
    const canvas = document.getElementById('usersChart');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    
    chartInstances.users = new Chart(ctx, {
        type: 'doughnut',
        data: {
            labels: ['Ativos', 'Inativos', 'Sem Créditos'],
            datasets: [{
                data: [0, 0, 0], // Será atualizado
                backgroundColor: [
                    'rgba(75, 192, 192, 0.8)',
                    'rgba(255, 206, 86, 0.8)',
                    'rgba(255, 99, 132, 0.8)'
                ]
            }]
        },
        options: {
            responsive: true,
            plugins: {
                title: {
                    display: true,
                    text: 'Status dos Usuários'
                }
            }
        }
    });
}

// ============== HANDLERS DE WEBSOCKET ESPECÍFICOS ==============

function handleSystemStatusUpdate(data) {
    adminData.stats = { ...adminData.stats, ...data };
    
    const currentPage = getCurrentAdminPage();
    if (currentPage === 'dashboard') {
        updateDashboardStats(adminData.stats);
    }
}

function handleUserActivityUpdate(data) {
    // Atualizar dados de usuários
    if (adminData.users) {
        const userIndex = adminData.users.findIndex(u => u.uid === data.uid);
        if (userIndex >= 0) {
            adminData.users[userIndex] = { ...adminData.users[userIndex], ...data };
        }
        
        const currentPage = getCurrentAdminPage();
        if (currentPage === 'dashboard') {
            updateActiveUsersDisplay();
        } else if (currentPage === 'users') {
            // Atualizar tabela de usuários se na página
            if (typeof updateUsersTable === 'function') {
                updateUsersTable();
            }
        }
    }
}

function handleCoffeeServedUpdate(data) {
    // Atualizar estatísticas de café
    if (adminData.stats.coffee) {
        adminData.stats.coffee.remaining = Math.max(0, adminData.stats.coffee.remaining - 1);
        adminData.stats.coffee.totalServed = (adminData.stats.coffee.totalServed || 0) + 1;
    }
    
    // Atualizar displays
    const currentPage = getCurrentAdminPage();
    if (currentPage === 'dashboard') {
        updateDashboardStats(adminData.stats);
    }
}

function handleLogEntryUpdate(data) {
    // Adicionar novo log ao início da lista
    if (adminData.logs) {
        adminData.logs.unshift(data);
        
        // Limitar a 100 logs na memória
        if (adminData.logs.length > 100) {
            adminData.logs = adminData.logs.slice(0, 100);
        }
        
        // Atualizar display se na página de logs
        const currentPage = getCurrentAdminPage();
        if (currentPage === 'logs') {
            updateLogsDisplay();
        } else if (currentPage === 'dashboard') {
            updateRecentActivity();
        }
    }
}

// ============== UTILITÁRIOS ESPECÍFICOS DO ADMIN ==============

function getActivityIcon(log) {
    if (typeof log === 'string') {
        if (log.includes('CAFÉ') || log.includes('COFFEE')) return '☕';
        if (log.includes('LOGIN')) return '🔑';
        if (log.includes('USUÁRIO') || log.includes('USER')) return '👤';
        if (log.includes('ERROR') || log.includes('ERRO')) return '❌';
        if (log.includes('WARN')) return '⚠️';
        return 'ℹ️';
    }
    
    if (log.level) {
        switch (log.level.toLowerCase()) {
            case 'error': return '❌';
            case 'warning': case 'warn': return '⚠️';
            case 'success': return '✅';
            default: return 'ℹ️';
        }
    }
    
    return 'ℹ️';
}

function formatLogTime(log) {
    if (typeof log === 'string') {
        // Se for string simples, usar timestamp atual
        return new Date().toLocaleTimeString('pt-BR', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
    }
    
    if (log.timestamp) {
        return new Date(log.timestamp).toLocaleTimeString('pt-BR', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
        });
    }
    
    return '--:--:--';
}

function getLogLevel(log) {
    if (typeof log === 'string') {
        if (log.includes('ERROR') || log.includes('ERRO')) return 'error';
        if (log.includes('WARN')) return 'warning';
        if (log.includes('SUCCESS') || log.includes('SUCESSO')) return 'success';
        return 'info';
    }
    
    return log.level || 'info';
}

function getLast7Days() {
    const days = [];
    for (let i = 6; i >= 0; i--) {
        const date = new Date();
        date.setDate(date.getDate() - i);
        days.push(date.toLocaleDateString('pt-BR', { 
            day: '2-digit', 
            month: '2-digit' 
        }));
    }
    return days;
}

function isActiveToday(timestamp) {
    if (!timestamp) return false;
    const today = new Date();
    today.setHours(0, 0, 0, 0);
    return timestamp >= today.getTime();
}

function setupAdminEventHandlers() {
    // Handler específico para comandos do admin via teclado
    document.addEventListener('keydown', function(e) {
        // Ctrl + R para refresh manual
        if (e.ctrlKey && e.key === 'r') {
            e.preventDefault();
            const currentPage = getCurrentAdminPage();
            
            switch (currentPage) {
                case 'dashboard':
                    loadDashboardData();
                    break;
                case 'users':
                    if (typeof loadUsers === 'function') loadUsers();
                    break;
                case 'logs':
                    loadLogs();
                    break;
                case 'stats':
                    loadStatsData();
                    break;
            }
            
            showAlert('Dados atualizados manualmente', 'info');
        }
        
        // Ctrl + S para servir café (apenas no dashboard)
        if (e.ctrlKey && e.key === 's' && getCurrentAdminPage() === 'dashboard') {
            e.preventDefault();
            serveCoffee();
        }
    });
}

// ============== EXPORTAR FUNÇÕES PARA USO GLOBAL ==============

window.serveCoffee = serveCoffee;
window.refillCoffee = refillCoffee;
window.resetSystem = resetSystem;
window.exportData = exportData;
window.toggleAutoRefresh = toggleAutoRefresh;
window.refreshActivity = refreshActivity;
window.clearLogs = clearLogs;
window.handleSystemStatusUpdate = handleSystemStatusUpdate;
window.handleUserActivityUpdate = handleUserActivityUpdate;
window.handleCoffeeServedUpdate = handleCoffeeServedUpdate;
window.handleLogEntryUpdate = handleLogEntryUpdate;