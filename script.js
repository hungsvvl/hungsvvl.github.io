// ============ 1. CẤU HÌNH FIREBASE ============
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
const firebaseConfig = {
  apiKey: "AIzaSyBKIkzONHTQZZcmQIkzG9avMM8kwG8Yzck",
  authDomain: "quanlychamcong-9dacd.firebaseapp.com",
  databaseURL: "https://quanlychamcong-9dacd-default-rtdb.firebaseio.com",
  projectId: "quanlychamcong-9dacd",
  storageBucket: "quanlychamcong-9dacd.firebasestorage.app",
  messagingSenderId: "550554398392",
  appId: "1:550554398392:web:ff0ef39ffd21e8c7ac0e4d",
  measurementId: "G-VDV8GZ5N01"
};
if (!firebase.apps.length) firebase.initializeApp(firebaseConfig);
const db = firebase.database();

// ============ 2. BIẾN TOÀN CỤC ============
let usersCache = {};
let usersTrashCache = {};
let newID = null;
let processedNewIDs = new Set();
let currentRole = 'Guest';
let chartPie = null, chartBar = null;
let confirmCallback = null;
let currentTab = null;            
let isOfflineMode = false;
window.tempReport = [];

// ============ 3. STATUS BAR (XỬ LÝ THANH TRẠNG THÁI) ============
const setStatusBarStyle = async (isDark) => {
    if (typeof Capacitor === 'undefined') return;
    const { StatusBar } = Capacitor.Plugins;
    
    try {
        await StatusBar.setOverlaysWebView({ overlay: true });
        if (isDark) {
            await StatusBar.setStyle({ style: 'DARK' }); 
            await StatusBar.setBackgroundColor({ color: '#1e293b' }); 
        } else {
            await StatusBar.setStyle({ style: 'LIGHT' });
            await StatusBar.setBackgroundColor({ color: '#00000000' }); 
        }
    } catch (e) {
        console.log("Lỗi set status bar:", e);
    }
};

// ============ 4. MẠNG & CHẾ ĐỘ OFFLINE ============
async function initNetwork() {
    if (typeof Capacitor === 'undefined') return;
    const { Network } = Capacitor.Plugins;

    const status = await Network.getStatus();
    if (!status.connected) {
        document.getElementById('offline-modal').style.display = 'flex';
    }

    Network.addListener('networkStatusChange', (status) => {
        if (!status.connected) {
            showToast('⚠️ Đã mất kết nối Internet', 'error');
            document.getElementById('offline-modal').style.display = 'flex';
        } else {
            // Có mạng lại -> Ẩn modal và tự động tải lại nếu đang offline
            document.getElementById('offline-modal').style.display = 'none';
            if (isOfflineMode) {
                isOfflineMode = false;
                location.reload();
            }
        }
    });
}

function openWifiSettings() {
    document.getElementById('offline-modal').style.display = 'none';
    showToast('Vui lòng bật Wifi/4G trong Cài đặt điện thoại', 'info');
}

function switchToOfflineMode() {
    isOfflineMode = true;
    document.getElementById('offline-modal').style.display = 'none';
    showToast('Đang xem dữ liệu cũ (Chế độ Offline)', 'warning');
    
    const cachedUsers = localStorage.getItem('cache_users');
    if (cachedUsers) usersCache = JSON.parse(cachedUsers);
    renderLoginDropdown();
    
    if (currentRole === 'Admin') {
        renderUserList(true);
        loadMonitor(true);
    } else if (currentRole === 'Emp') {
        const s = JSON.parse(localStorage.getItem('utc_session_v21') || '{}');
        if (s.id) loadEmpHistory(s.id, true);
    }
}

// ============ 5. PUSH NOTIFICATION (FCM) ============
function initPush() {
    console.log('--- Bắt đầu khởi tạo Push ---');
    if (typeof Capacitor === 'undefined') return;

    const PushNotifications = Capacitor.Plugins.PushNotifications;
    const FCM = Capacitor.Plugins.FCM;

    if (PushNotifications) {
        PushNotifications.createChannel({
            id: 'default',
            name: 'Thông báo chung',
            importance: 5,
            visibility: 1,
            vibration: true
        }).catch(err => console.log('Lỗi tạo channel:', err));
    }

    PushNotifications.addListener('registration', (token) => {
        console.log('FCM TOKEN:', token.value);
        if (FCM) {
            FCM.subscribeTo({ topic: 'admin_alerts' })
                .then(() => showToast('🔔 Đã kết nối kênh Admin', 'success'))
                .catch((err) => console.error('Lỗi đăng ký topic:', err));
        }
    });

    PushNotifications.addListener('registrationError', (error) => {
        console.error('Lỗi Push:', JSON.stringify(error));
    });

    PushNotifications.addListener('pushNotificationReceived', (notification) => {
        const modal = document.getElementById('new-id-modal');
        const isModalOpen = modal && modal.style.display === 'flex';
        
        if (isModalOpen && (notification.title.includes('chưa định danh') || notification.title.includes('vân tay'))) {
            return; // Chặn thông báo trùng lặp
        }
        showToast('🔔 ' + notification.title + ': ' + notification.body, 'success');
    });

    PushNotifications.addListener('pushNotificationActionPerformed', (notification) => {
        openTab('live');
    });
    
    PushNotifications.requestPermissions().then(result => {
        if (result.receive === 'granted') {
            PushNotifications.register();
        }
    });
}

// ============ 6. BIOMETRIC LOGIN (VÂN TAY) ============
async function biometricLogin() {
    if (typeof Capacitor === 'undefined') return;
    
    // Kiểm tra đã kích hoạt bằng mật khẩu chưa
    const isLinked = localStorage.getItem('biometric_linked');
    if (isLinked !== 'true') {
        return showToast('⚠️ Vui lòng đăng nhập bằng Mật Khẩu lần đầu để kích hoạt!', 'warning');
    }

    const NativeBiometric = Capacitor.Plugins.NativeBiometric;
    if (!NativeBiometric) return showToast('Lỗi Plugin', 'error');

    try {
        const result = await NativeBiometric.isAvailable();
        if (!result.isAvailable) return showToast('Máy không hỗ trợ vân tay', 'warning');

        await NativeBiometric.verifyIdentity({
            reason: "Quét vân tay Admin",
            title: "Xác thực",
            subtitle: "Đăng nhập quyền quản trị",
            description: "Vui lòng chạm vào cảm biến"
        });

        showToast('✅ Xác thực thành công!', 'success');
        setAppMode('Admin');
        db.ref('new_enroll').remove().catch(() => {});

    } catch (error) {
        console.error("Lỗi vân tay:", error);
    }
}

// ============ 7. ĐĂNG NHẬP & PHÂN QUYỀN ============
(function checkAutoLogin() {
    const saved = localStorage.getItem('utc_session_v21');
    if (saved) {
        try {
            const s = JSON.parse(saved);
            if (s.role === 'Admin') loginAdmin(true);
            else if (s.role === 'Emp' && s.id) loginEmp(true, s.id);
        } catch (e) {
            localStorage.removeItem('utc_session_v21');
        }
    }
})();

document.addEventListener('keydown', function (e) {
    if (e.key === 'Enter') {
        const loginPage = document.getElementById('login-page');
        if (loginPage && loginPage.style.display !== 'none') {
            const adminVisible = !document.getElementById('form-admin').classList.contains('hidden');
            if (adminVisible) loginAdmin();
            else loginEmp();
        }
    }
});

async function loginAdmin(isAuto = false) {
    if (isAuto) {
        setAppMode('Admin');
        return;
    }
    const u = document.getElementById('user').value;
    const p = document.getElementById('pass').value;
    
    // Offline mode check
    if (isOfflineMode && u === 'UTC' && p === 'admin') {
         setAppMode('Admin');
         return;
    }

    const snap = await db.ref('settings/admin_password').once('value');
    
    // Kiểm tra đăng nhập
    if (u === 'UTC' && p === (snap.val() || 'admin')) {
        
        // --- ĐOẠN CODE ĐÃ SỬA ---
        // Chỉ kích hoạt khi biến isNative = true (Nghĩa là đang chạy App)
        // Trên Web, biến này là false hoặc undefined -> Code trong if sẽ KHÔNG chạy
        if (typeof Capacitor !== 'undefined' && Capacitor.isNative) {
            localStorage.setItem('biometric_linked', 'true');
            showToast('Đã kích hoạt đăng nhập Vân Tay', 'success');
        }
        // ------------------------

        if (document.getElementById('remember-admin').checked)
            localStorage.setItem('utc_session_v21', JSON.stringify({ role: 'Admin' }));
        
        setAppMode('Admin');
        db.ref('new_enroll').remove().catch(() => {}); 
    } else {
        showToast('Sai thông tin!', 'error');
    }
}

function loginEmp(isAuto = false, autoId = null) {
    const id = isAuto ? autoId : document.getElementById('login-select').value;
    if (!id) return showToast('Vui lòng chọn tên', 'error');
    const info = getUserInfo(id);
    if (!isAuto && document.getElementById('remember-emp').checked)
        localStorage.setItem('utc_session_v21', JSON.stringify({ role: 'Emp', id: id }));
    setAppMode('Emp', info.name);
    loadEmpHistory(id, isOfflineMode);
}

function logout() {
    localStorage.removeItem('utc_session_v21');
    // localStorage.removeItem('biometric_linked'); // Bỏ comment nếu muốn hủy vân tay khi đăng xuất
    location.reload();
}

function setAppMode(role, name = 'Quản Trị') {
    currentRole = role;
    document.getElementById('login-page').style.display = 'none';
    document.getElementById('display-name').innerText = name;
    document.getElementById('display-role').innerText = role === 'Admin' ? 'Admin' : 'Nhân Viên';
    document.body.classList.remove('role-admin', 'role-emp');
    document.body.classList.add(role === 'Admin' ? 'role-admin' : 'role-emp');
    document.getElementById('view-emp').classList.add('hidden');
    document.getElementById('view-admin').classList.add('hidden');
    if (role === 'Admin') {
        document.getElementById('view-admin').classList.remove('hidden');
        openTab('dashboard');
    } else {
        document.getElementById('view-emp').classList.remove('hidden');
    }
}

// ============ 8. CÁC HÀM ĐỔI MẬT KHẨU (CÓ BẢO MẬT CẤP 2) ============
function openChangePassModal() {
    // Reset trắng các ô khi mở bảng
    document.getElementById('secret-input').value = '';
    document.getElementById('old-pass-input').value = '';
    document.getElementById('new-pass-input').value = '';
    document.getElementById('confirm-pass-input').value = '';
    
    document.getElementById('password-modal').style.display = 'flex';
}

function closeChangePassModal() {
    document.getElementById('password-modal').style.display = 'none';
}

async function saveNewPassword() {
    if (isOfflineMode) return showToast('Cần có mạng để đổi mật khẩu', 'warning');

    const secretAns = document.getElementById('secret-input').value.trim();
    const oldPass = document.getElementById('old-pass-input').value;
    const newPass = document.getElementById('new-pass-input').value;
    const confirmPass = document.getElementById('confirm-pass-input').value;

    // 1. Kiểm tra nhập thiếu
    if (!secretAns || !oldPass || !newPass || !confirmPass) {
        return showToast('Vui lòng nhập đầy đủ thông tin!', 'error');
    }

    // 2. KIỂM TRA CÂU TRẢ LỜI BÍ MẬT (QUAN TRỌNG)
    // Chuyển về chữ thường để "Nhom3" hay "nhom3" đều chấp nhận
    if (secretAns.toLowerCase() !== 'nhom3') {
        return showToast('❌ Câu trả lời bảo mật KHÔNG ĐÚNG!', 'error');
    }

    // 3. Kiểm tra logic mật khẩu
    if (newPass !== confirmPass) return showToast('Mật khẩu mới không khớp', 'error');
    if (newPass.length < 4) return showToast('Mật khẩu quá ngắn (>= 4 ký tự)', 'warning');

    try {
        // 4. Kiểm tra mật khẩu cũ trên Firebase
        const snap = await db.ref('settings/admin_password').once('value');
        const currentRealPass = snap.val() || 'admin';

        if (oldPass !== currentRealPass) {
            return showToast('Mật khẩu cũ không đúng!', 'error');
        }

        // 5. Lưu mật khẩu mới
        await db.ref('settings/admin_password').set(newPass);
        
        showToast('✅ Đổi mật khẩu thành công!', 'success');
        closeChangePassModal();

    } catch (error) {
        console.error(error);
        showToast('Lỗi hệ thống, thử lại sau', 'error');
    }
}

// ============ 9. DATA LOGIC (MONITOR, USERS, SALARY...) ============

// 1. Hàm tải dữ liệu giám sát
function loadMonitor(isOffline = false) {
    if (isOffline) {
        const cached = localStorage.getItem('cache_attendance');
        const data = cached ? JSON.parse(cached) : {};
        renderMonitorTable(data);
        return;
    }
    // Lắng nghe dữ liệu chấm công từ Firebase
    db.ref('attendance').limitToLast(50).on('value', (snap) => {
        const data = snap.val();
        if (data) localStorage.setItem('cache_attendance', JSON.stringify(data));
        renderMonitorTable(data);
    });
}

// 2. Hàm hiển thị bảng giám sát (Logic Mới: Vào/Ra)
// ============ TÌM HÀM renderMonitorTable VÀ DÁN ĐÈ ĐOẠN NÀY VÀO ============

function renderMonitorTable(data) {
    const tbody = document.getElementById('live-body');
    if (!tbody) return;
    tbody.innerHTML = '';

    if (!data) {
        tbody.innerHTML = "<tr><td colspan='4' style='text-align:center; color:#999; padding:20px'>Chưa có dữ liệu</td></tr>";
        return;
    }

    const STORAGE_BUCKET = "quanlychamcong-9dacd.firebasestorage.app";

    Object.entries(data).reverse().forEach(([key, log]) => {
        const info = getUserInfo(log.id);
        const [datePart, timePart] = log.timestamp.split(' ');

        // --- 1. XỬ LÝ TRẠNG THÁI (SỬA LOGIC Ở ĐÂY) ---
        let statusBadge = "";

        // ƯU TIÊN 1: Nếu không có tên -> Là NGƯỜI LẠ
        if (!info.name) {
            statusBadge = `<span class="badge" style="background-color: #ef4444; color: white; border: none;">NGƯỜI LẠ</span>`;
        }
        // ƯU TIÊN 2: Nếu có tên -> Hiện trạng thái Vào/Ra
        else if (log.status === "IN") {
            statusBadge = `<span class="badge" style="background-color: #10b981; color: white; border: none;">VÀO LÀM</span>`;
        } else if (log.status === "OUT") {
            statusBadge = `<span class="badge" style="background-color: #f59e0b; color: white; border: none;">RA VỀ</span>`;
        } else {
            statusBadge = `<span class="badge" style="background-color: #64748b; color: white;">Check-in</span>`;
        }

        // --- 2. CỘT TÊN NHÂN VIÊN (SỬA LOGIC Ở ĐÂY) ---
        let nameHtml = "";

        if (info.name) {
            // Có tên -> Hiện tên
            nameHtml = `<div class="compact-main">${info.name}</div>`;
        } else {
            // Không tên -> Hiện ID (Thay vì chữ "Chưa ĐK")
            nameHtml = `<div style="font-family:monospace; font-weight:700; color:#333; font-size:1rem">${log.id}</div>`;
        }

        let subHtml = !info.name ?
            `<div style="margin-top:4px"><button class="btn btn-outline" style="padding:2px 8px; font-size:0.65rem; height:auto" onclick="quickAdd('${log.id}')">➕ Thêm Tên</button></div>` :
            `<div class="compact-sub">${info.code !== '---' ? info.code : `ID: ${log.id}`}</div>`;

        if (log.auto_generated && info.name) nameHtml += ` <i class="fa-solid fa-bolt" style="color:#f59e0b; font-size:0.7rem"></i>`;

        // --- 3. CỘT THỜI GIAN ---
        let timeHtml = `<div class="compact-cell">
                            <div class="compact-time" style="font-size: 1.1rem; font-weight: bold; color: var(--text-main);">${timePart}</div>
                            <div style="margin-top:4px; display: flex; align-items: center; gap: 5px;">
                                ${statusBadge}
                                <span style="font-size:0.75rem; color: var(--text-sub);">(${datePart.substr(0,5)})</span>
                            </div>
                        </div>`;

        // --- 4. ẢNH CHECK-IN ---
        let imgHtml = '';
        if (log.image) {
            const path = encodeURIComponent('photos/' + log.image);
            const imgUrl = `https://firebasestorage.googleapis.com/v0/b/${STORAGE_BUCKET}/o/${path}?alt=media`;

            imgHtml = `<img src="${imgUrl}"
                        style="width: 60px; height: 60px; object-fit: cover; border-radius: 8px; border: 1px solid #e2e8f0; cursor: pointer; transform: rotate(180deg);"
                        onclick="window.open('${imgUrl}', '_blank')"
                        onerror="this.style.display='none'"
                        alt="Img">`;
        } else {
            imgHtml = `<span style="font-size:0.8rem; color:#cbd5e1; font-style:italic;">Không ảnh</span>`;
        }

        // --- 5. NÚT XÓA ---
        let delBtn = !isOfflineMode ?
            `<button class="btn btn-danger" style="padding:8px; width:32px; height:32px; border-radius:50%;" onclick="triggerDeleteLog('${key}')"><i class="fa-solid fa-trash" style="font-size:0.8rem"></i></button>` :
            `<i class="fa-solid fa-cloud-arrow-down" style="color:#ccc"></i>`;

        tbody.innerHTML += `<tr>
                                <td><div class="compact-cell">${nameHtml}${subHtml}</div></td>
                                <td>${timeHtml}</td>
                                <td style="text-align:center;">${imgHtml}</td>
                                <td style="text-align:center">${delBtn}</td>
                            </tr>`;
    });
}
// 3. Lắng nghe danh sách nhân viên từ Firebase
db.ref('users').on('value', (snap) => {
    usersCache = snap.val() || {};
    localStorage.setItem('cache_users', JSON.stringify(usersCache));

    // Cập nhật giao diện khi có dữ liệu user mới
    renderLoginDropdown();
    if (currentRole === 'Admin') {
        renderUserList();
        if(currentTab === 'live') loadMonitor(); // Load lại bảng giám sát để cập nhật tên
        loadTrash();
    }
    if (currentRole === 'Emp') {
        const s = JSON.parse(localStorage.getItem('utc_session_v21') || 'null');
        if (s) document.getElementById('display-name').innerText = getUserInfo(s.id).name || 'Nhân Viên';
    }
});

// 4. Hàm hiển thị danh sách nhân sự (Tab Nhân Sự)
function renderUserList(isOffline = false) {
    // Tự động tìm thẻ tbody chuẩn
    const tbody = document.getElementById('user-table-body') || document.querySelector('#users-list tbody');
    if (!tbody) { console.error("Lỗi HTML: Không tìm thấy bảng nhân sự"); return; }

    tbody.innerHTML = '';

    if (!usersCache || Object.keys(usersCache).length === 0) {
        tbody.innerHTML = '<tr><td colspan="4" style="text-align:center; padding:15px">Chưa có nhân sự</td></tr>';
        return;
    }

    Object.keys(usersCache).forEach((id) => {
        const info = getUserInfo(id);
        const tr = document.createElement('tr');
        tr.onclick = function() { fillForm(id); };
        tr.style.cursor = 'pointer';
        tr.innerHTML = `<td><span class="badge badge-id">${id}</span></td><td>${info.code}</td><td style="font-weight:600">${info.name}</td>`;
        tbody.appendChild(tr);
    });
}

// 5. Lịch sử cá nhân (Cho nhân viên)
function loadEmpHistory(id, isOffline = false) {
    if (isOffline) {
        const cached = localStorage.getItem('cache_attendance');
        const data = cached ? JSON.parse(cached) : {};
        renderEmpTable(id, data);
        return;
    }
    db.ref('attendance').limitToLast(100).on('value', (snap) => {
        const data = snap.val();
        if(data) localStorage.setItem('cache_attendance', JSON.stringify(data));
        renderEmpTable(id, data);
    });
}

function renderEmpTable(id, data) {
    const tb = document.querySelector('#emp-table tbody');
    if(!tb) return;
    tb.innerHTML = '';
    if (data)
        Object.values(data).reverse().forEach((log) => {
            if (log.id == id)
                tb.innerHTML += `<tr><td>${log.timestamp}</td><td><span class="badge badge-green">Thành công</span></td></tr>`;
        });
}

// 6. Xử lý người lạ (New Enroll)
db.ref('new_enroll').on('value', async (snap) => {
    const val = snap.val();
    if (val) {
        newID = val;
        let isDuplicate = false;
        // Logic chống spam chấm công liên tục
        const lastLogSnap = await db.ref('attendance').orderByChild('id').equalTo(String(val)).limitToLast(1).once('value');
        const lastLogData = lastLogSnap.val();
        if (lastLogData) {
            const k = Object.keys(lastLogData)[0];
            const [datePart, timePart] = lastLogData[k].timestamp.split(' ');
            const [day, month, year] = datePart.split('/');
            const [hour, minute, second] = timePart.split(':');
            const lastTime = new Date(year, month - 1, day, hour, minute, second);
            if ((new Date() - lastTime) < 60000) isDuplicate = true; // 60s cooldown
        }

        if (!isDuplicate) {
            // Không tự động push attendance nữa vì ESP32 đã push rồi
            // Chỉ hiện popup nếu là Admin
            processedNewIDs.add(val);
        }

        if (currentRole === 'Admin' && !isOfflineMode) {
            const modalId = document.getElementById('modal-id');
            if(modalId) modalId.innerText = newID;
            const modal = document.getElementById('new-id-modal');
            if(modal) modal.style.display = 'flex';
        }
    } else {
        const modal = document.getElementById('new-id-modal');
        if(modal) modal.style.display = 'none';
    }
});

function acceptNewID() {
  openTab('users');
  document.getElementById('inp-id').value = newID;
  document.getElementById('inp-code').focus();
  closeNewID();
}

function closeNewID() {
  document.getElementById('new-id-modal').style.display = 'none';
  db.ref('new_enroll').remove();
}

function quickAdd(id) {
    openTab('users');
    document.getElementById('inp-id').value = id;
    document.getElementById('inp-code').focus();
}

// 7. Dashboard & Biểu đồ
async function updateDashboard() {
    let data = {};
    if (isOfflineMode) {
        const c = localStorage.getItem('cache_attendance');
        data = c ? JSON.parse(c) : {};
    } else {
        const snap = await db.ref('attendance').limitToLast(200).once('value');
        data = snap.val() || {};
    }

    const logs = Object.values(data);
    const workTime = '08:00:00';
    let late = 0, onTime = 0, daysCount = {};
    const today = new Date();
    const todayString = `${String(today.getDate()).padStart(2, '0')}/${String(today.getMonth() + 1).padStart(2, '0')}/${today.getFullYear()}`;
    let todayTotal = 0, todayOnTime = 0, todayLate = 0;

    logs.forEach((log) => {
        const [date, time] = log.timestamp.split(' ');

        // Logic đếm Vào/Ra thay vì Muộn/Sớm
        if (log.status === 'OUT') late++; else onTime++;

        daysCount[date] = (daysCount[date] || 0) + 1;
        if (date === todayString) {
            todayTotal++;
            if (log.status === 'OUT') todayLate++; else todayOnTime++;
        }
    });

    // Cập nhật số liệu Dashboard
    const elTotal = document.getElementById('total-today'); if(elTotal) elTotal.innerText = todayTotal;
    const elOnTime = document.getElementById('on-time-today'); if(elOnTime) { elOnTime.innerText = todayOnTime; elOnTime.parentElement.querySelector('div:last-child').innerText = "Vào Làm"; }
    const elLate = document.getElementById('late-today'); if(elLate) { elLate.innerText = todayLate; elLate.parentElement.querySelector('div:last-child').innerText = "Ra Về"; }

    // Vẽ biểu đồ Pie
    const ctxPie = document.getElementById('chartPie');
    if (ctxPie) {
        if (chartPie) chartPie.destroy();
        chartPie = new Chart(ctxPie, {
            type: 'doughnut',
            data: { labels: ['Vào Làm', 'Ra Về'], datasets: [{ data: [onTime, late], backgroundColor: ['#10b981', '#f59e0b'] }] },
            options: { maintainAspectRatio: false, plugins: { legend: { position: 'bottom' } } }
        });
    }

    // Vẽ biểu đồ Cột
    const ctxBar = document.getElementById('chartBar');
    if (ctxBar) {
        if (chartBar) chartBar.destroy();
        const labels = Object.keys(daysCount).slice(-7);
        chartBar = new Chart(ctxBar, {
            type: 'bar',
            data: { labels: labels, datasets: [{ label: 'Lượt Chấm', data: labels.map((d) => daysCount[d]), backgroundColor: '#4f46e5', borderRadius: 4 }] },
            options: { maintainAspectRatio: false, scales: { y: { beginAtZero: true } } }
        });
    }
}

// 8. Hàm Tính Lương Theo Giờ (MỚI)
async function calcSalary() {
    if (isOfflineMode) return showToast('Chức năng này cần Online', 'warning');

    const dStart = document.getElementById('date-start').value;
    const dEnd = document.getElementById('date-end').value;
    const salaryValInput = document.getElementById('salary-val');

    if(!salaryValInput) return showToast('Lỗi HTML: Mất ô nhập lương', 'error');
    const salaryPerHour = Number(salaryValInput.value || 0);

    if (!dStart || !dEnd) return showToast('Chọn khoảng ngày!', 'error');

    const start = new Date(dStart); start.setHours(0, 0, 0, 0);
    const end = new Date(dEnd); end.setHours(23, 59, 59, 999);

    showToast('Đang tải dữ liệu...', 'info');

    try {
        const [usersSnap, attendanceSnap] = await Promise.all([
            db.ref('users').once('value'),
            db.ref('attendance').once('value')
        ]);

        const usersMap = usersSnap.val() || {};
        const attendanceData = attendanceSnap.val();

        const tbody = document.getElementById('salary-table').querySelector('tbody');
        tbody.innerHTML = '';

        let totalAll = 0;
        window.tempReport = [];
        let worked = {};

        if (attendanceData) {
            Object.values(attendanceData).forEach((log) => {
                if(!log.timestamp || !log.timestamp.includes('/')) return;
                const [datePart, timePart] = log.timestamp.split(' ');
                const [day, month, year] = datePart.split('/');
                const logDateObj = new Date(`${year}-${month}-${day}`);
                if (logDateObj >= start && logDateObj <= end) {
                    if (!worked[log.id]) worked[log.id] = {};
                    if (!worked[log.id][datePart]) worked[log.id][datePart] = [];
                    worked[log.id][datePart].push(log.timestamp);
                }
            });
        }

        if (Object.keys(worked).length === 0) {
            tbody.innerHTML = "<tr><td colspan='4' style='text-align:center; padding:20px'>Không có dữ liệu chấm công.</td></tr>";
            return;
        }

        Object.keys(worked).forEach((id) => {
            const user = usersMap[id];
            if (!user || !user.name) return;

            let totalHours = 0;
            Object.keys(worked[id]).forEach((dateStr) => {
                const times = worked[id][dateStr];
                if (times.length < 2) return;
                times.sort((a, b) => {
                    const parseT = (t) => {
                        const [d, time] = t.split(' ');
                        const [dd, mm, yy] = d.split('/');
                        return new Date(`${yy}-${mm}-${dd}T${time}`);
                    };
                    return parseT(a) - parseT(b);
                });
                const parseToMs = (tStr) => {
                    const [d, time] = tStr.split(' ');
                    const [dd, mm, yy] = d.split('/');
                    return new Date(`${yy}-${mm}-${dd}T${time}`).getTime();
                }
                const tIn = parseToMs(times[0]);
                const tOut = parseToMs(times[times.length - 1]);
                const hours = (tOut - tIn) / (1000 * 60 * 60);
                totalHours += hours;
            });

            totalHours = Math.round(totalHours * 10) / 10;
            const money = Math.round(totalHours * salaryPerHour);
            totalAll += money;

            window.tempReport.push({ Mã: user.code || '---', Tên: user.name, "Tổng Giờ": totalHours, "Thực Lĩnh": money });
            tbody.innerHTML += `<tr><td><span class="badge badge-id">${user.code || '---'}</span></td><td>${user.name}</td><td style="font-weight:bold; color: var(--primary);">${totalHours} h</td><td style="color:var(--success); font-weight:800">${money.toLocaleString()} đ</td></tr>`;
        });

        document.getElementById('total-money').innerText = totalAll.toLocaleString() + ' đ';
        showToast('Đã tính xong!', 'success');

    } catch (error) {
        console.error("Lỗi tính lương:", error);
        showToast('Lỗi: ' + error.message, 'error');
    }
}

async function exportExcel() {
    if (!window.tempReport || window.tempReport.length === 0) return showToast('Không có dữ liệu!', 'error');
    const ws = XLSX.utils.json_to_sheet(window.tempReport);
    const wb = XLSX.utils.book_new();
    XLSX.utils.book_append_sheet(wb, ws, 'BangLuong');
    const fileName = `BangLuong_UTC_${Date.now()}.xlsx`;
    try {
        if (typeof Capacitor !== 'undefined') {
            const Filesystem = Capacitor.Plugins.Filesystem;
            if (!Filesystem) { XLSX.writeFile(wb, fileName); return; }
            const excelBase64 = XLSX.write(wb, { bookType: 'xlsx', type: 'base64' });
            await Filesystem.writeFile({ path: fileName, data: excelBase64, directory: 'DOCUMENTS', recursive: true });
            showToast(`✅ Đã lưu: Documents/${fileName}`, 'success');
        } else {
            XLSX.writeFile(wb, fileName);
            showToast('Đang tải file xuống...', 'success');
        }
    } catch (e) {
        showToast('Lỗi lưu file: ' + e.message, 'error');
    }
}

function triggerDeleteLog(key) {
    if(isOfflineMode) return showToast('Không thể xóa khi Offline', 'warning');
    showConfirmDialog('Xóa dòng này vào thùng rác?', () => {
        db.ref('attendance/' + key).once('value', (s) => {
            db.ref('trash/' + key).set(s.val()).then(() => {
                db.ref('attendance/' + key).remove();
                showToast('Đã xóa', 'success');
            });
        });
    });
}

function triggerDeleteData() {
    if(isOfflineMode) return showToast('Không thể xóa khi Offline', 'warning');
    const dStart = document.getElementById('date-start').value;
    const dEnd = document.getElementById('date-end').value;
    if (!dStart || !dEnd) return showToast('Chọn khoảng ngày!', 'error');

    showConfirmDialog(`Chuyển dữ liệu từ ${dStart} đến ${dEnd} vào thùng rác?`, async () => {
        const start = new Date(dStart); start.setHours(0, 0, 0, 0);
        const end = new Date(dEnd); end.setHours(23, 59, 59, 999);
        const snap = await db.ref('attendance').once('value');
        const data = snap.val();
        if (!data) return showToast('Không tìm thấy dữ liệu', 'info');

        let updates = {}, count = 0;
        Object.entries(data).forEach(([k, log]) => {
            const [dStr] = log.timestamp.split(' ');
            const [dd, mm, yyyy] = dStr.split('/');
            const logDate = new Date(`${yyyy}-${mm}-${dd}`);
            if (logDate >= start && logDate <= end) {
                updates['trash/' + k] = log; updates['attendance/' + k] = null; count++;
            }
        });
        if (count > 0) {
            await db.ref().update(updates);
            showToast(`Đã xóa ${count} dòng!`, 'success');
            document.getElementById('salary-table').querySelector('tbody').innerHTML = '';
        } else showToast('Không tìm thấy dữ liệu', 'info');
    });
}

function triggerDeleteUser() {
    if(isOfflineMode) return;
    const id = document.getElementById('inp-id').value.trim();
    if (!id) return showToast('Chưa chọn nhân sự', 'error');
    showConfirmDialog('Xoá nhân viên này?', () => {
        const userData = usersCache[id];
        if (!userData) { showToast('Không tồn tại', 'error'); return; }
        db.ref('users_trash/' + id).set(userData).then(() => db.ref('users/' + id).remove()).then(() => { showToast('Đã chuyển vào thùng rác', 'success'); resetForm(); });
    });
}

function loadTrash() {
    db.ref('trash').on('value', (snap) => {
        const tbody = document.getElementById('trash-body');
        tbody.innerHTML = '';
        const data = snap.val();
        if (!data) { tbody.innerHTML = "<tr><td colspan='3' style='text-align:center; color:#999; padding:20px'>Thùng rác trống</td></tr>"; return; }
        Object.entries(data).forEach(([key, log]) => {
            const info = getUserInfo(log.id);
            let nameHtml = info.name ? `<b>${info.name}</b>` : `<span class="badge badge-yellow">Chưa ĐK</span>`;
            tbody.innerHTML += `<tr><td>${nameHtml} <div style="font-size:0.75rem;color:#666">${info.code}</div></td><td>${log.timestamp}</td><td style="text-align:center"><button class="btn btn-outline" style="padding:6px;margin-right:5px" onclick="restoreLog('${key}')" title="Khôi phục"><i class="fa-solid fa-rotate-left"></i></button><button class="btn btn-danger" style="padding:6px" onclick="permDeleteLog('${key}')" title="Xóa vĩnh viễn"><i class="fa-solid fa-xmark"></i></button></td></tr>`;
        });
    });
}

function restoreLog(key) {
    db.ref('trash/' + key).once('value', (snap) => { if (snap.val()) { db.ref('attendance/' + key).set(snap.val()).then(() => { db.ref('trash/' + key).remove(); showToast('Đã khôi phục', 'success'); }); } });
}

function permDeleteLog(key) { showConfirmDialog('Xóa vĩnh viễn? Không thể lấy lại!', () => { db.ref('trash/' + key).remove(); showToast('Đã xóa vĩnh viễn', 'success'); }); }
function emptyTrash() { showConfirmDialog('Dọn sạch thùng rác?', () => { db.ref('trash').remove(); showToast('Đã dọn sạch', 'success'); }); }

db.ref('users_trash').on('value', (snap) => {
    usersTrashCache = snap.val() || {};
    renderUserTrashList();
});

function renderUserTrashList() {
    const tbody = document.querySelector('#users-trash-list tbody'), countSpan = document.getElementById('deleted-count');
    if (!tbody) return;
    tbody.innerHTML = "";
    const list = usersTrashCache || {}, count = Object.keys(list).length;
    if (countSpan) countSpan.textContent = `(${count})`;
    if (count === 0) { tbody.innerHTML = "<tr><td colspan='4' style='text-align:center; padding:12px; color:#999'>Không có nhân sự đã xoá</td></tr>"; return; }
    Object.entries(list).forEach(([id, u]) => {
        const name = typeof u === 'string' ? u : (u.name || ''), code = typeof u === 'string' ? '---' : (u.code || '---');
        tbody.innerHTML += `<tr><td><span class="badge badge-id">${id}</span></td><td>${code}</td><td>${name}</td><td style="text-align:center"><button class="btn btn-outline" style="padding:4px 8px; font-size:0.7rem" onclick="restoreUser('${id}')">Khôi phục</button></td></tr>`;
    });
}

function restoreUser(id) {
    const data = usersTrashCache[id];
    if (!data) return showToast('Lỗi dữ liệu', 'error');
    db.ref('users/' + id).set(data).then(() => db.ref('users_trash/' + id).remove()).then(() => showToast('Đã khôi phục', 'success'));
}

// ============ 10. HELPERS ============
function showToast(msg, type = 'info') {
    const container = document.getElementById('toast-container');
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    let icon = type === 'error' ? '<i class="fa-solid fa-circle-exclamation" style="color:var(--danger)"></i>' :
               type === 'success' ? '<i class="fa-solid fa-circle-check" style="color:var(--success)"></i>' :
               '<i class="fa-solid fa-circle-info" style="color:var(--primary)"></i>';
    toast.innerHTML = `${icon}<div><div class="toast-title">${type === 'error' ? 'Lỗi' : type === 'success' ? 'Thành công' : 'Thông báo'}</div><div class="toast-msg">${msg}</div></div>`;
    container.appendChild(toast);
    setTimeout(() => toast.remove(), 3000);
}

function showConfirmDialog(msg, callback) {
    document.getElementById('confirm-msg').innerText = msg;
    document.getElementById('confirm-modal').style.display = 'flex';
    confirmCallback = callback;
}

function closeConfirmModal() {
    document.getElementById('confirm-modal').style.display = 'none';
    confirmCallback = null;
}

document.getElementById('btn-confirm-yes').onclick = function () {
    if (confirmCallback) confirmCallback();
    closeConfirmModal();
};

function toggleTheme() {
    const body = document.body;
    const isDark = body.getAttribute('data-theme') === 'dark';
    if (isDark) {
        body.removeAttribute('data-theme');
        setStatusBarStyle(false); 
    } else {
        body.setAttribute('data-theme', 'dark');
        setStatusBarStyle(true);
    }
}

function getCurrentDateTime() {
    const now = new Date();
    return `${String(now.getDate()).padStart(2, '0')}/${String(now.getMonth() + 1).padStart(2, '0')}/${now.getFullYear()} ${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
}

function getUserInfo(id) {
    const u = usersCache[id];
    if (!u) return { name: null, code: '---' };
    if (typeof u === 'string') return { name: u, code: '---' };
    return { name: u.name, code: u.code || '---' };
}

function renderLoginDropdown() {
    const sel = document.getElementById('login-select');
    sel.innerHTML = '<option value="">-- Chọn tên --</option>';
    Object.keys(usersCache).forEach((id) => {
        const i = getUserInfo(id);
        if (i.name) {
            const opt = document.createElement('option');
            opt.value = id;
            opt.text = i.name;
            sel.appendChild(opt);
        }
    });
}

function switchRole(role, btn) {
    document.querySelectorAll('.role-btn').forEach((b) => b.classList.remove('active'));
    if (btn) btn.classList.add('active');
    document.getElementById('form-emp').classList.add('hidden');
    document.getElementById('form-admin').classList.add('hidden');
    document.getElementById('form-' + role).classList.remove('hidden');
}

function openTab(tabName) {
    if (currentTab === tabName) return;
    currentTab = tabName;
    document.querySelectorAll('.tab-content').forEach((el) => {
        el.classList.remove('active');
        el.classList.add('hidden');
    });
    
    const contentId = tabName === 'trash' || tabName === 'settings' ? 
        (tabName === 'trash' ? 'tab-trash-detail' : 'tab-settings') : 'tab-' + tabName;
    
    const target = document.getElementById(contentId);
    if (target) {
        target.classList.remove('hidden');
        setTimeout(() => target.classList.add('active'), 10);
    }
    
    document.querySelectorAll('.nav-link, .b-nav-item').forEach((el) => el.classList.remove('active'));
    const pcBtn = document.getElementById('pc-' + tabName);
    const mobBtn = document.getElementById('mob-' + tabName);
    if (pcBtn) pcBtn.classList.add('active');
    if (mobBtn) mobBtn.classList.add('active');

    if (tabName === 'dashboard') updateDashboard();
    if (tabName === 'live') loadMonitor(isOfflineMode);
    if (tabName === 'trash') loadTrash();
    if (tabName === 'users') {
        renderUserList(isOfflineMode);
        renderUserTrashList();
    }
}

function openTrashView() {
    openTab('trash');
}

function fillForm(id) {
    const info = getUserInfo(id);
    document.getElementById('inp-id').value = id;
    document.getElementById('inp-code').value = info.code !== '---' ? info.code : '';
    document.getElementById('inp-name').value = info.name;
    document.getElementById('btn-del-user').classList.remove('hidden');
}

function saveUser() {
    if(isOfflineMode) return showToast('Offline không thể lưu', 'warning');
    const id = document.getElementById('inp-id').value.trim();
    const name = document.getElementById('inp-name').value.trim();
    const code = document.getElementById('inp-code').value.trim();
    const isNew = document.getElementById('btn-del-user').classList.contains('hidden');

    if (!id || !name) return showToast('Thiếu thông tin!', 'error');
    if (isNew && usersCache[id]) return showToast('Nhân viên này đã tồn tại.', 'error');
    
    for (const [k, v] of Object.entries(usersCache)) {
        const vCode = typeof v === 'string' ? null : v.code;
        if (k !== id && vCode && code && vCode.toUpperCase() === code.toUpperCase()) {
            return showToast('Trùng Mã NV!', 'error');
        }
    }

    db.ref('users/' + id).set({ name, code }).then(() => {
        showToast('Lưu thành công', 'success');
        resetForm();
    });
}

function switchUserView(type) {
    const mainWrap = document.getElementById('users-main-wrapper');
    const trashWrap = document.getElementById('users-trash-wrapper');
    const btnMain = document.getElementById('btn-users-main');
    const btnTrash = document.getElementById('btn-users-trash');

    if (!mainWrap || !trashWrap || !btnMain || !btnTrash) return;

    if (type === 'trash') {
        mainWrap.classList.add('hidden');
        trashWrap.classList.remove('hidden');
        btnMain.classList.remove('active');
        btnTrash.classList.add('active');
    } else {
        trashWrap.classList.add('hidden');
        mainWrap.classList.remove('hidden');
        btnTrash.classList.remove('active');
        btnMain.classList.add('active');
    }
}

function resetForm() {
    document.getElementById('inp-id').value = '';
    document.getElementById('inp-name').value = '';
    document.getElementById('inp-code').value = '';
    document.getElementById('btn-del-user').classList.add('hidden');
}

const todayInit = new Date();
const firstDayInit = new Date(todayInit.getFullYear(), todayInit.getMonth(), 1);
document.getElementById('date-start').valueAsDate = firstDayInit;
document.getElementById('date-end').valueAsDate = todayInit;

// ============ 11. KHỞI CHẠY ỨNG DỤNG (ĐÃ SỬA LỖI MẤT NÚT) ============
setTimeout(() => {
    // Lấy tên nền tảng: 'web', 'android', hoặc 'ios'
    const platform = Capacitor.getPlatform(); 
    console.log("Platform hiện tại:", platform);

    if (platform === 'web') {
        // Chỉ ẩn nút vân tay nếu ĐÚNG LÀ đang chạy trên trình duyệt Web
        const bioBtn = document.querySelector('button[onclick="biometricLogin()"]');
        if (bioBtn) bioBtn.style.display = 'none';
    } else {
        // Nếu là Android/iOS -> Giữ nguyên nút vân tay và chạy các plugin
        setStatusBarStyle(false); 
        initPush();               
    }

    // Khởi tạo kiểm tra mạng (Chạy trên cả Web và App)
    initNetwork();            
}, 1000);