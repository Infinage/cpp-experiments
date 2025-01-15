const getURLCount = async() => {
  let urlCount = -1;
  try {
    const response = await fetch('/ping');
    const data = await response.json();
    if (data.count !== undefined) urlCount = data.count
  } 
  finally { document.getElementById('urlCount').innerHTML = `<sup>${urlCount}</sup>`; }
}

const shortenURL = async () => {
  const longUrl = document.getElementById('longUrl').value;
  if (!longUrl.trim()) {
    showNotification("Please enter a valid URL.");
    return; 
  }

  try {
    const response = await fetch('/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: longUrl })
    });

    const data = await response.json();
    
    if (data.short_url !== undefined && response.ok) {
      const resultDiv = document.getElementById('result');
      const shortUrl = window.location.origin + '/' + data.key;
      resultDiv.innerHTML = `
        <span class="url">URL${response.status == 201? '*': ''}: ${shortUrl}</span>
        <span class="button" onclick="copyToClipboard('${shortUrl}')">📋</span>
        <span class="button" onclick="deleteURL('${data.key}')">🗑️</span>
      `;
    } else {
      showNotification("Error: " + data || "Unable to shorten URL.");
    }
  } 
  catch (error) { showNotification("Error: " + error.message); } 
  finally { getURLCount(); }
}

const deleteURL = async (shortUrl) => {
  try {
    const response = await fetch(`/${shortUrl}`, { method: 'DELETE' });

    if (response.status === 200) {
      showNotification("URL deleted successfully!");
      document.getElementById('result').innerHTML = "";
    } else if (response.status === 404) {
      showNotification("URL not found!");
      document.getElementById('result').innerHTML = "";
    } else {
      showNotification("An error occurred while deleting the URL.");
    }
  } catch (error) {
    showNotification("Error: " + error);
  } finally {
    getURLCount();
  }
};

const copyToClipboard = async (text) => {
  try {
    await navigator.clipboard.writeText(text);
    showNotification("Copied to clipboard!");
  } catch (err) {
    showNotification("Failed to copy.");
  }
}

// Hold reference to timeout to support multiple notifications
let snackbarTimeout;

// Function to show notification 
const showNotification = (message, delayMS=2500) => {
    const snackbar = document.getElementById('snackbar');
    snackbar.innerText = message;
    snackbar.classList.add("show");
    clearTimeout(snackbarTimeout);
    snackbarTimeout = setTimeout(closeNotification, delayMS); 
};

// Function to hide the snackbar
const closeNotification = () => {
    const snackbar = document.getElementById('snackbar');
    snackbar.innerText = "";
    snackbar.classList.remove("show");
};
