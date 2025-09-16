/*
==================================================
APP.JS - SCRIPT PRINCIPAL DA APLICAÇÃO
Funcionalidades compartilhadas entre páginas
==================================================
*/

// Variáveis globais
let currentUser = null;
let websocket = null;
let isAuthenticated = false;
let autoRefreshInterval = null;

// Configurações
const CONFIG = {
    websocket: {
        reconnectInterval: 5000,
        maxReconnectAttempts: 10
    },
    alerts: {
        autoHideDelay: 5000
    },
    api: {
        timeout: 10000
    }
};

// ============== INICIALIZAÇÃO ==============

document.addEventListener('DOMContentLoaded', function() {
    initializeApp();
});

function initializeApp() {
    console.log('Inicializando aplicação...');

    // Verificar autenticação
    checkAuth();

    // Configurar handlers globais
    setupGlobalEventHandlers();

    // Tentar conectar WebSocket se autenticado
    if (isAuthenticated) {
        initWebSocket();
    }
}

// ============== AUTENTICAÇÃO ==============

async function checkAuth() {
    try {
        const response = await fetch('/auth/check');
        const data = await response.json();

        if (data.authenticated) {
            isAuthenticated = true;
            currentUser = {
                username: data.username,
                role: data.role,
                sessionTime: data.sessionTime
            };

            console.log('Usuário autenticado:', currentUser);
            updateUserInfo();

            // Iniciar WebSocket aqui, pois agora sabemos que está autenticado
            initWebSocket();

            const currentPath = window.location.pathname;
            if (currentPath === '/login' || currentPath === '/') {
                redirectToDashboard();
            }

        } else {
            isAuthenticated = false;
            currentUser = null;

            const currentPath = window.location.pathname;
            if (currentPath !== '/login' && currentPath !== '/') {
                window.location.href = '/';
            }
        }
    } catch (error) {
        console.error('Erro ao verificar autenticação:', error);
        isAuthenticated = false;
        currentUser = null;
    }
}

function redirectToDashboard() {
    if (currentUser) {
        const dashboardUrl = currentUser.role === 'Admin' ? '/admin/dashboard' : '/user/dashboard';
        window.location.href = dashboardUrl;
    }
}

function updateUserInfo() {
    if (!currentUser) return;

    const userNameElements = document.querySelectorAll('.user-name');
    const userRoleElements = document.querySelectorAll('.user-role');
    const userAvatarElements = document.querySelectorAll('.user-avatar');

    userNameElements.forEach(el => {
        el.textContent = currentUser.username;
    });

    userRoleElements.forEach(el => {
        el.textContent = currentUser.role;
    });

    userAvatarElements.forEach(el => {
        el.textContent = currentUser.username.charAt(0).toUpperCase();
    });
}

async function logout() {
    try {
        const response = await fetch('/auth/logout', {
            method: 'POST'
        });

        if (response.ok) {
            isAuthenticated = false;
            currentUser = null;

            if (websocket) {
                websocket.close();
                websocket = null;
            }

            if (autoRefreshInterval) {
                clearInterval(autoRefreshInterval);
                autoRefreshInterval = null;
            }

            window.location.href = '/';
        } else {
            showAlert('Erro ao fazer logout', 'error');
        }
    } catch (error) {
        console.error('Erro no logout:', error);
        showAlert('Erro de conexão no logout', 'error');
    }
}

// ============== WEBSOCKET ==============

function initWebSocket() {
    if (!isAuthenticated || (websocket && websocket.readyState === WebSocket.OPEN)) {
        return;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;

    console.log('Conectando WebSocket:', wsUrl);

    try {
        websocket = new WebSocket(wsUrl);

        websocket.onopen = function(event) {
            console.log('WebSocket conectado');
            showAlert('Conectado ao sistema em tempo real', 'success');

            sendWebSocketMessage('auth', {
                username: currentUser?.username,
                role: currentUser?.role
            });
        };

        websocket.onmessage = function(event) {
            try {
                const data = JSON.parse(event.data);
                handleWebSocketMessage(data);
            } catch (error) {
                console.error('Erro ao processar mensagem WebSocket:', error);
            }
        };

        websocket.onclose = function(event) {
            console.log('WebSocket desconectado:', event.code, event.reason);

            if (isAuthenticated && !event.wasClean) {
                showAlert('Conexão perdida. Tentando reconectar...', 'warning');
                setTimeout(() => {
                    if (isAuthenticated) {
                        initWebSocket();
                    }
                }, CONFIG.websocket.reconnectInterval);
            }
        };

        websocket.onerror = function(error) {
            console.error('Erro no WebSocket:', error);
            showAlert('Erro na conexão em tempo real', 'error');
        };

    } catch (error) {
        console.error('Erro ao criar WebSocket:', error);
    }
}

function sendWebSocketMessage(type, data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        const message = {
            type: type,
            timestamp: Date.now(),
            data: data
        };
        websocket.send(JSON.stringify(message));
    }
}

function handleWebSocketMessage(message) {
    console.log('Mensagem WebSocket recebida:', message);

    switch (message.type) {
        case 'system_status':
            updateSystemStatus(message.data);
            break;
        case 'user_activity':
            updateUserActivity(message.data);
            break;
        case 'coffee_served':
            handleCoffeeServed(message.data);
            break;
        case 'rfid_event':
            handleRFIDEvent(message.data);
            break;
        case 'log_entry':
            handleLogEntry(message.data);
            break;
        case 'alert':
            showAlert(message.data.message, message.data.type);
            break;
        case 'new_rfid_uid': // ADDED THIS CASE
            if (typeof window.handleNewRfidUid === 'function') {
                window.handleNewRfidUid(message.data.uid);
            }
            break;
        default:
            console.log('Tipo de mensagem não reconhecido:', message.type);
    }
}

// ============== SISTEMA DE ALERTAS ==============

function showAlert(message, type = 'info', autoHide = true) {
    const container = document.getElementById('alertContainer');
    if (!container) {
        console.warn('Container de alertas não encontrado');
        return;
    }

    const alertDiv = document.createElement('div');
    alertDiv.className = `alert alert-${type}`;
    alertDiv.innerHTML = `
        <div class="alert-icon">${getAlertIcon(type)}</div>
        <div class="alert-content">
            <div class="alert-title">${getAlertTitle(type)}</div>
            <div class="alert-message">${escapeHtml(message)}</div>
        </div>
        <button class="alert-close" onclick="closeAlert(this)">&times;</button>
    `;

    container.appendChild(alertDiv);

    if (autoHide) {
        setTimeout(() => {
            closeAlert(alertDiv.querySelector('.alert-close'));
        }, CONFIG.alerts.autoHideDelay);
    }
}

function closeAlert(button) {
    const alert = button.closest('.alert');
    if (alert) {
        alert.style.opacity = '0';
        alert.style.transform = 'translateX(100%)';
        setTimeout(() => {
            if (alert.parentNode) {
                alert.parentNode.removeChild(alert);
            }
        }, 300);
    }
}

function getAlertIcon(type) {
    const icons = { success: '✅', error: '❌', warning: '⚠️', info: 'ℹ️' };
    return icons[type] || icons.info;
}

function getAlertTitle(type) {
    const titles = { success: 'Sucesso', error: 'Erro', warning: 'Atenção', info: 'Informação' };
    return titles[type] || titles.info;
}

// ... (Rest of your app.js file remains the same from here)

// ============== MODAIS ==============

function showConfirmDialog(title, message, onConfirm, onCancel = null) {
    const modal = document.getElementById('confirmModal');
    if (!modal) {
        console.error('Modal de confirmação não encontrado');
        return;
    }

    const titleElement = modal.querySelector('#confirmTitle');
    const messageElement = modal.querySelector('#confirmMessage');

    if (titleElement) titleElement.textContent = title;
    if (messageElement) messageElement.textContent = message;

    window.confirmAction = onConfirm;
    window.cancelAction = onCancel;

    modal.classList.add('show');
}

function closeConfirmModal() {
    const modal = document.getElementById('confirmModal');
    if (modal) {
        modal.classList.remove('show');
    }
    window.confirmAction = null;
    window.cancelAction = null;
}

function executeConfirmedAction() {
    if (typeof window.confirmAction === 'function') {
        window.confirmAction();
    }
    closeConfirmModal();
}

// ============== LOADING ==============

function showLoading(show = true) {
    const overlay = document.getElementById('loadingOverlay');
    if (overlay) {
        overlay.style.display = show ? 'flex' : 'none';
    }
}

// ============== UTILITÁRIOS ==============

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function formatDateTime(timestamp) {
    if (!timestamp) return 'N/A';

    const date = new Date(timestamp);
    const now = new Date();
    const diffMs = now - date;

    if (date.toDateString() === now.toDateString()) {
        return date.toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' });
    }
    if (diffMs < 7 * 24 * 60 * 60 * 1000) {
        const days = Math.floor(diffMs / (24 * 60 * 60 * 1000));
        return `${days}d atrás`;
    }
    return date.toLocaleDateString('pt-BR');
}

function formatUptime(uptimeMs) {
    const seconds = Math.floor(uptimeMs / 1000);
    const minutes = Math.floor(seconds / 60);
    const hours = Math.floor(minutes / 60);
    const days = Math.floor(hours / 24);

    if (days > 0) return `${days}d ${hours % 24}h ${minutes % 60}m`;
    if (hours > 0) return `${hours}h ${minutes % 60}m`;
    if (minutes > 0) return `${minutes}m ${seconds % 60}s`;
    return `${seconds}s`;
}

// ============== HANDLERS DE EVENTOS GLOBAIS ==============

function setupGlobalEventHandlers() {
    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            closeConfirmModal();
            document.querySelectorAll('.modal-overlay.show').forEach(modal => modal.classList.remove('show'));
        }
    });

    document.addEventListener('click', function(e) {
        if (e.target.classList.contains('modal-overlay')) {
            e.target.classList.remove('show');
        }
    });

    window.addEventListener('beforeunload', function(e) {
        if (websocket) websocket.close();
    });
}

// ============== HANDLERS WEBSOCKET ESPECÍFICOS ==============

function updateSystemStatus(data) {
    if (typeof handleSystemStatusUpdate === 'function') {
        handleSystemStatusUpdate(data);
    }
}

function updateUserActivity(data) {
    if (typeof handleUserActivityUpdate === 'function') {
        handleUserActivityUpdate(data);
    }
}

function handleCoffeeServed(data) {
    showAlert(`Café servido para ${data.userName}`, 'success');
    if (typeof handleCoffeeServedUpdate === 'function') {
        handleCoffeeServedUpdate(data);
    }
}

function handleRFIDEvent(data) {
    const message = `${data.userName}: ${data.action}`;
    const type = data.success ? 'success' : 'warning';
    showAlert(message, type);
    if (typeof handleRFIDEventUpdate === 'function') {
        handleRFIDEventUpdate(data);
    }
}

function handleLogEntry(data) {
    if (typeof handleLogEntryUpdate === 'function') {
        handleLogEntryUpdate(data);
    }
}

// ============== API HELPERS ==============

async function apiRequest(url, options = {}) {
    try {
        const response = await fetch(url, {
            headers: { 'Content-Type': 'application/json' },
            ...options
        });
        if (response.status === 401) {
            window.location.href = '/';
            return null;
        }
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        return await response.json();
    } catch (error) {
        console.error('Erro na requisição API:', error);
        throw error;
    }
}

// ============== EXPORTAR FUNÇÕES GLOBAIS ==============

window.logout = logout;
window.showAlert = showAlert;
window.closeAlert = closeAlert;
window.showConfirmDialog = showConfirmDialog;
window.closeConfirmModal = closeConfirmModal;
window.executeConfirmedAction = executeConfirmedAction;
window.showLoading = showLoading;
window.escapeHtml = escapeHtml;
window.formatDateTime = formatDateTime;
window.formatUptime = formatUptime;
window.apiRequest = apiRequest;
window.checkAuth = checkAuth;