const myGames = [
  { 
      title: "Pong", 
      link: "games/pong/index.html", 
      tags: ["gamepad", "multiplayer"] 
  },
];

const myMovies = [
  { title: "The Shining", rating: 4 },
];

const myBooks = [
{ title: "Torto Arado", author: "Itamar Vieira Junior", rating: 3 },
{ title: "Vidas Secas", author: "Graciliano Ramos", rating: 4 },
{ title: "Véspera", author: "Carla Madeira", rating: 5 },
{ title: "In Memoriam", author: "Alice Winn", rating: 2 },
{ title: "A Hora da Estrela", author: "Clarice Lispector", rating: 4 },
{ title: "A Diferença Invisível", author: "Julie Dachez", rating: 3 },
{ title: "Normal People", author: "Sally Rooney", rating: 2 },
{ title: "A Metamorfose", author: "Franz Kafka", rating: 4 },
{ title: "Remina", author: "Junji Ito", rating: 4 },
{ title: "Uzumaki", author: "Junji Ito", rating: 4 },
{ title: "Morte e Vida Severina", author: "João Cabral de Melo Neto", rating: 5 },
{ title: "The Fire Next Time", author: "James Baldwin", rating: 5 },
{ title: "I Have No Mouth & I Must Scream", author: "Harlan Ellison", rating: 1 },
{ title: "Things We Lost in the Fire", author: "Mariana Enriquez", rating: 4 },
{ title: "Demolidor: O Homem Sem Medo", author: "Frank Miller", rating: 3 },
{ title: "Tudo é Rio", author: "Carla Madeira", rating: 3 },
{ title: "A Natureza da Mordida", author: "Carla Madeira", rating: 5 },
{ title: "Kophee", author: "Guilherme Match", rating: 3 },
{ title: "Sentimento do Mundo", author: "Carlos Drummond de Andrade", rating: 4 },
{ title: "Goodnight Punpun Vol. 1", author: "Inio Asano", rating: 3 },
{ title: "The Lost Weekend", author: "Charles Jackson", rating: 4 },
{ title: "The Last Question", author: "Isaac Asimov", rating: 3 },
{ title: "Your Brain Is a Time Machine", author: "Dean Buonomano", rating: 2 },
{ title: "No Longer Human", author: "Osamu Dazai", rating: 4 },
{ title: "Os 7 Maridos de Evelyn Hugo", author: "Taylor Jenkins Reid", rating: 3 },
{ title: "Every Frame a Rembrandt", author: "Andrew Laszlo", rating: 4 },
{ title: "Goodbye, Eri", author: "Tatsuki Fujimoto", rating: 5 },
{ title: "Just Listen to the Song", author: "Tatsuki Fujimoto", rating: 3 },
{ title: "Noites Brancas", author: "Fyodor Dostoevsky", rating: 1 },
{ title: "Maus", author: "Art Spiegelman", rating: 4 },
{ title: "All About Love", author: "Bell Hooks", rating: 4 },
{ title: "Memórias do Subsolo", author: "Fyodor Dostoevsky", rating: 5 },
{ title: "Le Mythe de Sisyphe", author: "Albert Camus", rating: 5 },
{ title: "La Peste", author: "Albert Camus", rating: 4 },
{ title: "Poemas, Sonetos e Baladas", author: "Vinicius de Moraes", rating: 5 },
{ title: "A Rosa do Povo", author: "Carlos Drummond de Andrade", rating: 5 },
{ title: "L'Étranger", author: "Albert Camus", rating: 5 },

];

const iconMap = {
  "gamepad": "fa-solid fa-gamepad",
  "multiplayer": "fa-solid fa-user-group",
};

let booksLoaded = false;
let moviesLoaded = false;
let gamesLoaded = false;

function openTab(tabId) {
	localStorage.setItem('activeTab', tabId);

  const contents = document.querySelectorAll('.tab-content');
  contents.forEach(content => content.classList.remove('active'));
  
  const activeTab = document.getElementById(tabId);
  if (activeTab) activeTab.classList.add('active');

  const buttons = document.querySelectorAll('.tab-btn');
  buttons.forEach(btn => btn.classList.remove('active'));

  const activeBtn = Array.from(buttons).find(btn => 
    btn.getAttribute('onclick').includes(`'${tabId}'`)
  );
  if (activeBtn) activeBtn.classList.add('active');

  if (tabId === 'books' && !booksLoaded) {
    loadBooks();
    booksLoaded = true;
  } 
  else if (tabId === 'movies' && !moviesLoaded) {
    loadMovies();
    moviesLoaded = true;
  }
  else if (tabId === 'games' && !gamesLoaded) {
    loadGames();
    gamesLoaded = true;
  }
}

function loadGames() {
  const grid = document.getElementById('game-grid');
  if(!grid) return;
  grid.innerHTML = '';

  myGames.forEach(game => {
    const imagePath = `assets/thumbs/${sanitizeFilename(game.title)}.jpg`;

    const badgesHtml = (game.tags || []).map(tag => {
      const iconClass = iconMap[tag.toLowerCase()] || 'fa-solid fa-question';
      return `<i class="${iconClass}" title="${tag}"></i>`;
    }).join('');

    const card = document.createElement('a');
    card.className = 'card game-card';
    card.href = game.link;
    card.target = "_blank";

    card.innerHTML = `
      <div class="thumb" style="background-image: url('${imagePath}');">
        <div class="play-overlay">
          <i class="fa-solid fa-play"></i>
        </div>
      </div>
			<div class="card-info" style="align-items: stretch;"> <div class="game-header">
        <h3>${game.title}</h3>
        <div class="game-badges">
          ${badgesHtml}
        </div>
        </div>
      </div>
    `;
    grid.appendChild(card);
  });
}

function loadMovies() {
    const grid = document.getElementById('movie-grid');
    if(!grid) return;
    grid.innerHTML = '';

    myMovies.forEach(movie => {
        const imagePath = `assets/movies/${sanitizeFilename(movie.title)}.jpg`;
        
        const card = createCard({
            title: movie.title,
            subtitle: "",
            imagePath: imagePath,
            rating: movie.rating,
            isPoster: true
        });
        grid.appendChild(card);
    });
}

function loadBooks() {
  const grid = document.getElementById('book-grid');
  if (!grid) return;
  grid.innerHTML = '';

  myBooks.forEach(book => {
    const safeFilename = sanitizeFilename(book.title);
    const card = createCard({
      title: book.title,
      subtitle: book.author,
      imagePath: `assets/books/${safeFilename}.jpg`,
      rating: book.rating,
      isPoster: true
    });
    grid.appendChild(card);
  });
}

function createCard({ title, subtitle, imagePath, rating, isPoster = false }) {
  const div = document.createElement('div');
  div.className = 'card';
  
  const thumbClass = isPoster ? 'thumb poster-thumb' : 'thumb';

  const stars = '★'.repeat(rating) + '☆'.repeat(5 - rating);

  div.innerHTML = `
  <div class="${thumbClass}" style="background-image: url(&quot;${imagePath}&quot);"></div>
  <div class="card-info">
    <h3>${title}</h3>
    <span class="author">${subtitle}</span>
    <div class="rating" title="${rating}/5">${stars}</div>
  </div>
  `;
  return div;
}

function sanitizeFilename(name) {
  return name.replace(/[:/\\?%*|"<>]/g, '').trim();
}

document.addEventListener('DOMContentLoaded', () => {
  const savedTab = localStorage.getItem('activeTab') || 'home';
  openTab(savedTab);
});
