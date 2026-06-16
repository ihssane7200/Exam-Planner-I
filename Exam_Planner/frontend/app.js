// ============================================================================
// Configuration - URL DE TON BACKEND
// ============================================================================
const API_BASE = 'https://exam-planner-i-production.up.railway.app';

// ============================================================================
// Couleurs pour les matières
// ============================================================================
const SUBJECT_COLORS = [
    '#4f46e5', '#06b6d4', '#10b981', '#f59e0b',
    '#ef4444', '#8b5cf6', '#ec4899', '#14b8a6'
];

// ============================================================================
// Initialisation
// ============================================================================
document.addEventListener('DOMContentLoaded', () => {
    checkHealth();
    initForm();
});

// ============================================================================
// Vérifier si le serveur est en ligne
// ============================================================================
async function checkHealth() {
    try {
        const response = await fetch(`${API_BASE}/api/health`);
        const data = await response.json();
        
        const statusEl = document.getElementById('statusIndicator');
        if (statusEl) {
            statusEl.className = 'status online';
            statusEl.querySelector('.status-text').textContent = 
                'En ligne (IA prête)';
        }
    } catch (err) {
        const statusEl = document.getElementById('statusIndicator');
        if (statusEl) {
            statusEl.className = 'status offline';
            statusEl.querySelector('.status-text').textContent = 'Hors ligne';
        }
    }
}

// ============================================================================
// Initialiser le formulaire
// ============================================================================
function initForm() {
    const form = document.getElementById('planForm');
    if (form) {
        form.addEventListener('submit', async (e) => {
            e.preventDefault();
            await generatePlan();
        });
    }
    
    // Bouton pour générer
    const btn = document.getElementById('generateBtn');
    if (btn) {
        btn.addEventListener('click', generatePlan);
    }
}

// ============================================================================
// Récupérer les matières du formulaire
// ============================================================================
function getSubjects() {
    const subjectCards = document.querySelectorAll('.subject-card');
    const subjects = [];
    
    subjectCards.forEach(card => {
        const nameInput = card.querySelector('.subject-name');
        const chaptersInput = card.querySelector('.chapters-input');
        
        if (nameInput && nameInput.value.trim()) {
            const name = nameInput.value.trim();
            const chaptersText = chaptersInput ? chaptersInput.value.trim() : '';
            
            const chapters = chaptersText
                .split(',')
                .map(c => c.trim())
                .filter(c => c.length > 0)
                .map(c => ({ name: c, difficulty: 'moyen' }));
            
            if (chapters.length === 0) {
                chapters.push({ name: 'Tous les chapitres', difficulty: 'moyen' });
            }
            
            subjects.push({ name, chapters });
        }
    });
    
    return subjects;
}

// ============================================================================
// Ajouter une matière
// ============================================================================
function addSubject() {
    const container = document.getElementById('subjectsList');
    if (!container) return;
    
    const subjectCount = container.querySelectorAll('.subject-card').length + 1;
    const color = SUBJECT_COLORS[(subjectCount - 1) % SUBJECT_COLORS.length];
    
    const card = document.createElement('div');
    card.className = 'subject-card';
    card.style.borderLeftColor = color;
    
    card.innerHTML = `
        <span style="background:${color};color:white;padding:2px 8px;border-radius:4px;font-size:12px;">#${subjectCount}</span>
        <input type="text" class="subject-name" placeholder="Nom de la matière (ex: Mathématiques)" style="border-left:3px solid ${color};">
        <textarea class="chapters-input" placeholder="Chapitres séparés par des virgules..." rows="2"></textarea>
        <button type="button" onclick="this.parentElement.remove()" style="color:red;background:none;border:none;cursor:pointer;">🗑️</button>
    `;
    
    container.appendChild(card);
}

// Add subject on button click
document.addEventListener('DOMContentLoaded', () => {
    const addBtn = document.getElementById('addSubject');
    if (addBtn) {
        addBtn.addEventListener('click', addSubject);
    }
    // Add first subject by default
    addSubject();
});
// ============================================================================
// Générer le planning
// ============================================================================
async function generatePlan() {
    const totalDays = parseInt(document.getElementById('totalDays')?.value || '7');
    const hoursPerDay = parseFloat(document.getElementById('hoursPerDay')?.value || '4');
    const subjects = getSubjects();
    
    if (subjects.length === 0) {
        // Essayer le champ simple
        const simpleInput = document.getElementById('subjects');
        if (simpleInput && simpleInput.value.trim()) {
            const simpleSubjects = simpleInput.value.split(',').map(s => ({
                name: s.trim(),
                chapters: [{ name: 'Tous les chapitres', difficulty: 'moyen' }]
            }));
            subjects.push(...simpleSubjects);
        } else {
            showToast('Veuillez entrer au moins une matière', 'error');
            return;
        }
    }
    
    // Afficher loading
    showLoading(true);
    
    try {
        const response = await fetch(`${API_BASE}/api/generate-plan`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                user_id: 'demo_user',
                user_name: 'Étudiant',
                subjects: subjects,
                days_remaining: totalDays,
                hours_per_day: hoursPerDay,
                preferences: ''
            })
        });
        
        const data = await response.json();
        
        if (!response.ok) {
            throw new Error(data.error || 'Erreur lors de la génération');
        }
        
        // Adapter le format de la réponse
        const adaptedData = {
            success: true,
            data: {
                plan: data.plan,
                strategy: data.strategy,
                estimated_improvement: data.estimated_improvement,
                total_days: data.plan ? data.plan.length : 0,
                hours_per_day: hoursPerDay
            }
        };
        
        displayPlan(adaptedData.data);
        showToast('Planning généré avec succès !', 'success');
        
    } catch (err) {
        console.error('Erreur:', err);
        showToast(err.message || 'Erreur de connexion au serveur', 'error');
    } finally {
        showLoading(false);
    }
}

// ============================================================================
// Afficher le planning
// ============================================================================
function displayPlan(planData) {
    const container = document.getElementById('planContainer');
    const resultSection = document.getElementById('resultSection');
    
    if (resultSection) resultSection.classList.remove('hidden');
    if (!container) return;
    
    const plan = planData.plan || [];
    
    container.innerHTML = '';
    
    // Afficher stratégie
    if (planData.strategy) {
        const strategyDiv = document.createElement('div');
        strategyDiv.style.cssText = 'background:#f0fdf4; border:1px solid #86efac; padding:15px; border-radius:8px; margin-bottom:20px;';
        strategyDiv.innerHTML = `
            <strong>📋 Stratégie :</strong> ${escapeHtml(planData.strategy)}
            ${planData.estimated_improvement ? `<br><strong>📈 Amélioration estimée :</strong> ${escapeHtml(planData.estimated_improvement)}` : ''}
        `;
        container.appendChild(strategyDiv);
    }
    
    // Afficher chaque jour
    plan.forEach((day, index) => {
        const dayCard = document.createElement('div');
        dayCard.style.cssText = 'border:1px solid #ddd; border-radius:8px; padding:15px; margin-bottom:12px; background:#fff;';
        
        const sessions = day.sessions || [];
        let sessionsHtml = '';
        
        sessions.forEach(session => {
            const color = SUBJECT_COLORS[index % SUBJECT_COLORS.length];
            sessionsHtml += `
                <div style="border-left:3px solid ${color}; padding:8px; margin:8px 0; background:#f9fafb;">
                    <strong>${escapeHtml(session.subject)}</strong> — ${escapeHtml(session.chapter)}<br>
                    ⏱️ ${session.duration || session.duration_minutes || '?'} min |
                    🎯 Priorité: ${escapeHtml(session.priority || 'moyenne')} |
                    📝 ${escapeHtml(session.type || 'cours')}
                </div>
            `;
        });
        
        dayCard.innerHTML = `
            <h3>📅 Jour ${day.day} — ${escapeHtml(day.date || '')}</h3>
            <p><strong>Focus :</strong> ${escapeHtml(day.focus || 'Non spécifié')}</p>
            <p><strong>Heures :</strong> ${day.total_hours || '?'}h</p>
            ${sessionsHtml}
            <p style="font-style:italic; color:#6b7280;">💡 ${escapeHtml(day.daily_tip || '')}</p>
        `;
        
        container.appendChild(dayCard);
    });
    
    window.scrollTo({ top: container.offsetTop - 50, behavior: 'smooth' });
}

// ============================================================================
// Utilitaires
// ============================================================================
function showLoading(show) {
    const overlay = document.getElementById('loadingOverlay');
    if (overlay) {
        overlay.classList.toggle('hidden', !show);
    }
}

function showToast(message, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) {
        alert(message);
        return;
    }
    
    const toast = document.createElement('div');
    const colors = {
        success: '#10b981',
        error: '#ef4444',
        warning: '#f59e0b',
        info: '#3b82f6'
    };
    
    toast.style.cssText = `
        background: ${colors[type] || colors.info};
        color: white;
        padding: 12px 20px;
        border-radius: 8px;
        margin-bottom: 10px;
        animation: slideIn 0.3s ease;
    `;
    toast.textContent = message;
    
    container.appendChild(toast);
    
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transition = 'opacity 0.3s';
        setTimeout(() => toast.remove(), 300);
    }, 4000);
}

function escapeHtml(text) {
    if (!text) return '';
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

// ============================================================================
// Exposer les fonctions globalement
// ============================================================================
window.generatePlan = generatePlan;