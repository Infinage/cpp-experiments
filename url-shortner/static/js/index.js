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
    alert("Please enter a valid URL.");
    return; 
  }

  try {
    const response = await fetch('http://localhost:8080/', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url: longUrl })
    });

    const data = await response.json();
    
    if (data.short_url !== undefined && response.ok) {
      const resultDiv = document.getElementById('result');
      resultDiv.innerHTML = `
        <span class="url">URL: ${data.short_url}</span>
        <span class="button" onclick="copyToClipboard('${data.short_url}')">📋</span>
        <span class="button" onclick="deleteURL('${data.key}')">🗑️</span>
      `;
    } else {
      alert("Error: " + data || "Unable to shorten URL.");
    }
  } 
  catch (error) { alert("Error: " + error.message); } 
  finally { getURLCount(); }
}

const deleteURL = async (shortUrl) => {
  try {
    const response = await fetch(`http://localhost:8080/${shortUrl}`, { method: 'DELETE' });

    if (response.status === 200) {
      alert("URL deleted successfully!");
      document.getElementById('result').innerHTML = "";
    } else if (response.status === 404) {
      alert("URL not found!");
      document.getElementById('result').innerHTML = "";
    } else {
      alert("An error occurred while deleting the URL.");
    }
  } catch (error) {
    alert("Error: " + error);
  } finally {
    getURLCount();
  }
};

const copyToClipboard = async (text) => {
  try {
    await navigator.clipboard.writeText(text);
    alert("Copied to clipboard!");
  } catch (error) {
    alert("Failed to copy: " + err);
  }
}
