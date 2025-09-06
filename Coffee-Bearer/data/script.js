        async function atualizarStatus() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                
                document.getElementById('total-usuarios').textContent = data.total_usuarios;
                document.getElementById('total-cafes').textContent = data.total_cafes_servidos;
                
                const statusDiv = document.getElementById('status-sistema');
                const eventoDiv = document.getElementById('ultimo-evento');
                
                if (data.sistema_ocupado) {
                    statusDiv.className = 'status-sistema status-ocupado';
                    statusDiv.querySelector('strong').textContent = 'Sistema Ocupado';
                } else {
                    statusDiv.className = 'status-sistema';
                    statusDiv.querySelector('strong').textContent = 'Sistema Ativo';
                }
                eventoDiv.textContent = data.ultimo_evento || 'Aguardando eventos...';
            } catch (error) { console.error('Erro ao atualizar status:', error); }
        }
        
        async function listarUsuarios() {
            try {
                const response = await fetch('/api/usuarios');
                const data = await response.json();
                const listaDiv = document.getElementById('usuarios-lista');
                
                if (data.usuarios.length === 0) {
                    listaDiv.innerHTML = '<div style="text-align: center; color: #666; padding: 20px;">Nenhum usuário cadastrado</div>';
                } else {
                    listaDiv.innerHTML = "";
                    data.usuarios.forEach(usuario => {
                        const userDiv = document.createElement('div');
                        userDiv.className = 'usuario-item';
                        userDiv.innerHTML = `
                            <div class="usuario-info">
                                <div class="usuario-nome">${usuario.nome}</div>
                                <div class="usuario-uid">${usuario.uid}</div>
                            </div>
                            <button class="btn btn-danger" onclick="removerUsuario('${usuario.uid}')">Remover</button>`;
                        listaDiv.appendChild(userDiv);
                    });
                }
            } catch (error) { console.error('Erro ao listar usuários:', error); alert('Erro ao carregar lista de usuários'); }
        }
        
        async function adicionarUsuario(event) {
            event.preventDefault();
            const uid = document.getElementById('uid').value.trim().toUpperCase();
            const nome = document.getElementById('nome').value.trim();
            if (!uid || !nome) { alert('Por favor, preencha todos os campos'); return; }
            try {
                const response = await fetch('/api/usuarios', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ uid: uid, nome: nome })
                });
                const result = await response.json();
                if (result.success) {
                    alert('Usuário adicionado com sucesso!');
                    document.getElementById('uid').value = "";
                    document.getElementById('nome').value = "";
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao adicionar usuário:', error); alert('Erro ao adicionar usuário'); }
        }
        
        async function removerUsuario(uid) {
            if (!confirm('Tem certeza que deseja remover este usuário?')) { return; }
            try {
                const response = await fetch('/api/usuarios', {
                    method: 'DELETE',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ uid: uid })
                });
                const result = await response.json();
                if (result.success) {
                    alert('Usuário removido com sucesso!');
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao remover usuário:', error); alert('Erro ao remover usuário'); }
        }
        
        async function servirCafeManual() {
            try {
                const response = await fetch('/api/servir-cafe', { method: 'POST' });
                const result = await response.json();
                if (result.success) {
                    alert('Café servido manualmente!');
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao servir café:', error); alert('Erro ao servir café'); }
        }
        
        async function confirmarLimpeza() {
            const confirmacao = prompt('ATENÇÃO: Isso apagará TODOS os dados!\\n\\nPara confirmar, digite: LIMPAR TUDO');
            if (confirmacao !== 'LIMPAR TUDO') { alert('Operação cancelada'); return; }
            try {
                const response = await fetch('/api/limpar-dados', { method: 'DELETE' });
                const result = await response.json();
                if (result.success) {
                    alert('Todos os dados foram apagados!');
                    listarUsuarios();
                    atualizarStatus();
                } else { alert('Erro: ' + result.message); }
            } catch (error) { console.error('Erro ao limpar dados:', error); alert('Erro ao limpar dados'); }
        }
        
        async function fazerBackup() {
            try {
                const response = await fetch('/api/backup');
                const data = await response.json();
                if (data.usuarios.length === 0) { alert('Não há dados para fazer backup'); return; }
                let backupText = '// BACKUP DOS DADOS - ' + new Date().toLocaleString() + '\\n\\n';
                data.usuarios.forEach(usuario => { backupText += `ADD ${usuario.uid} ${usuario.nome}\\n`; });
                const blob = new Blob([backupText.replace(/\\n/g, '\\r\\n')], { type: 'text/plain' });
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = 'backup_cafeteira_' + new Date().toISOString().slice(0, 10) + '.txt';
                document.body.appendChild(a);
                a.click();
                window.URL.revokeObjectURL(url);
                document.body.removeChild(a);
                alert('Backup realizado com sucesso!');
            } catch (error) { console.error('Erro ao fazer backup:', error); alert('Erro ao fazer backup'); }
        }
        
        setInterval(atualizarStatus, 5000);
        document.addEventListener('DOMContentLoaded', function() {
            atualizarStatus();
            listarUsuarios();
        });
