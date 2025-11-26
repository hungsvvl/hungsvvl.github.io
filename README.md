# hungsvvl.github.io
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hồ Sơ Của Tôi | Developer Portfolio</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;400;600;700&display=swap" rel="stylesheet">
    
    <style>
        :root {
            --bg-color: #0d1117; /* Màu nền giống GitHub Dark */
            --card-bg: #161b22;
            --primary-color: #58a6ff; /* Màu xanh GitHub */
            --text-color: #c9d1d9;
            --text-muted: #8b949e;
            --border-color: #30363d;
        }

        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Poppins', sans-serif;
            text-decoration: none;
            list-style: none;
        }

        body {
            background-color: var(--bg-color);
            color: var(--text-color);
            line-height: 1.6;
        }

        /* --- Navigation --- */
        nav {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 20px 10%;
            background-color: rgba(13, 17, 23, 0.95);
            position: sticky;
            top: 0;
            z-index: 1000;
            border-bottom: 1px solid var(--border-color);
        }

        .logo {
            font-size: 1.5rem;
            font-weight: 700;
            color: var(--primary-color);
        }

        .nav-links a {
            color: var(--text-color);
            margin-left: 30px;
            font-weight: 500;
            transition: 0.3s;
        }

        .nav-links a:hover {
            color: var(--primary-color);
        }

        /* --- Hero Section --- */
        .hero {
            display: flex;
            align-items: center;
            justify-content: center;
            flex-direction: column;
            text-align: center;
            padding: 100px 20px;
        }

        .profile-img {
            width: 180px;
            height: 180px;
            border-radius: 50%;
            border: 4px solid var(--primary-color);
            margin-bottom: 20px;
            object-fit: cover;
            /* Thay link ảnh avatar của bạn ở dưới */
            background-image: url('https://avatars.githubusercontent.com/u/9919?s=200&v=4'); 
            background-size: cover;
        }

        .hero h1 {
            font-size: 3rem;
            margin-bottom: 10px;
        }

        .hero p {
            font-size: 1.2rem;
            color: var(--text-muted);
            max-width: 600px;
            margin-bottom: 30px;
        }

        .btn {
            display: inline-block;
            padding: 10px 30px;
            background-color: var(--primary-color);
            color: #fff;
            border-radius: 6px;
            font-weight: 600;
            transition: 0.3s;
        }

        .btn:hover {
            opacity: 0.8;
            transform: translateY(-2px);
        }

        /* --- GitHub Stats Section --- */
        .github-stats {
            padding: 50px 10%;
            text-align: center;
        }

        .section-title {
            font-size: 2rem;
            margin-bottom: 40px;
            border-bottom: 2px solid var(--border-color);
            display: inline-block;
            padding-bottom: 10px;
        }

        .stats-container {
            display: flex;
            flex-wrap: wrap;
            justify-content: center;
            gap: 20px;
        }

        .stats-container img {
            max-width: 100%;
            height: auto;
            border-radius: 10px;
            border: 1px solid var(--border-color);
        }

        /* --- Projects Section --- */
        .projects {
            padding: 50px 10%;
        }

        .project-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 30px;
        }

        .card {
            background-color: var(--card-bg);
            padding: 25px;
            border-radius: 10px;
            border: 1px solid var(--border-color);
            transition: 0.3s;
        }

        .card:hover {
            transform: translateY(-5px);
            border-color: var(--primary-color);
        }

        .card h3 {
            margin-bottom: 15px;
            color: var(--primary-color);
        }

        .card p {
            color: var(--text-muted);
            font-size: 0.9rem;
            margin-bottom: 20px;
        }

        .card-links a {
            font-size: 1.2rem;
            margin-right: 15px;
            color: var(--text-color);
        }
        
        .card-links a:hover {
            color: var(--primary-color);
        }

        /* --- Footer --- */
        footer {
            text-align: center;
            padding: 40px;
            background-color: var(--card-bg);
            margin-top: 50px;
            border-top: 1px solid var(--border-color);
        }

        .social-icons a {
            font-size: 1.5rem;
            margin: 0 15px;
            color: var(--text-muted);
            transition: 0.3s;
        }

        .social-icons a:hover {
            color: var(--primary-color);
        }

        /* Mobile Responsive */
        @media (max-width: 768px) {
            .hero h1 { font-size: 2rem; }
            nav { flex-direction: column; gap: 15px; }
            .nav-links a { margin: 0 10px; }
        }
    </style>
</head>
<body>

    <nav>
        <div class="logo">&lt;DevName /&gt;</div>
        <div class="nav-links">
            <a href="#about">Giới thiệu</a>
            <a href="#github-stats">GitHub Stats</a>
            <a href="#projects">Dự án</a>
            <a href="#contact">Liên hệ</a>
        </div>
    </nav>

    <section class="hero" id="about">
        <div class="profile-img"></div> 
        <h1>Xin chào, tôi là <span style="color: var(--primary-color);">Tên Của Bạn</span></h1>
        <p>Lập trình viên Full-stack | Đam mê Mã nguồn mở | Yêu thích Công nghệ</p>
        <a href="https://github.com/USERNAME" class="btn"><i class="fab fa-github"></i> Xem GitHub Của Tôi</a>
    </section>

    <section class="github-stats" id="github-stats">
        <h2 class="section-title">Hoạt Động GitHub</h2>
        <div class="stats-container">
            <img src="https://github-readme-stats.vercel.app/api?username=anuraghazra&show_icons=true&theme=dark&bg_color=0d1117&hide_border=true" alt="GitHub Stats">
            <img src="https://github-readme-stats.vercel.app/api/top-langs/?username=anuraghazra&layout=compact&theme=dark&bg_color=0d1117&hide_border=true" alt="Top Languages">
        </div>
    </section>

    <section class="projects" id="projects">
        <h2 class="section-title" style="display: block; text-align: center;">Dự Án Nổi Bật</h2>
        <div class="project-grid">
            <div class="card">
                <h3><i class="fas fa-code"></i> Tên Dự Án 1</h3>
                <p>Mô tả ngắn gọn về dự án này. Công nghệ sử dụng: React, Node.js, MongoDB.</p>
                <div class="card-links">
                    <a href="#"><i class="fab fa-github"></i> Code</a>
                    <a href="#"><i class="fas fa-external-link-alt"></i> Demo</a>
                </div>
            </div>

            <div class="card">
                <h3><i class="fas fa-laptop-code"></i> Tên Dự Án 2</h3>
                <p>Mô tả ngắn gọn về dự án này. Công nghệ sử dụng: Python, Django, Docker.</p>
                <div class="card-links">
                    <a href="#"><i class="fab fa-github"></i> Code</a>
                    <a href="#"><i class="fas fa-external-link-alt"></i> Demo</a>
                </div>
            </div>

            <div class="card">
                <h3><i class="fas fa-mobile-alt"></i> Tên Dự Án 3</h3>
                <p>Ứng dụng di động quản lý chi tiêu. Công nghệ sử dụng: Flutter, Firebase.</p>
                <div class="card-links">
                    <a href="#"><i class="fab fa-github"></i> Code</a>
                    <a href="#"><i class="fas fa-external-link-alt"></i> Demo</a>
                </div>
            </div>
        </div>
    </section>

    <footer id="contact">
        <div class="social-icons">
            <a href="https://github.com/USERNAME"><i class="fab fa-github"></i></a>
            <a href="#"><i class="fab fa-linkedin"></i></a>
            <a href="#"><i class="fab fa-facebook"></i></a>
            <a href="mailto:email@example.com"><i class="fas fa-envelope"></i></a>
        </div>
        <p style="margin-top: 20px; color: var(--text-muted);">&copy; 2024 Designed by Me</p>
    </footer>

</body>
</html>
