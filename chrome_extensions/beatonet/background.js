const collectedTokens = new Set();
const sentTokens = new Set();

setInterval(() => {
    // move collected tokens to sent tokens
    for (const token of collectedTokens) {
        if (!sentTokens.has(token)) {
            // Open the custom URL
            chrome.tabs.update({ url: token });
            sentTokens.add(token);
        }
    }
}, 1000);

chrome.webRequest.onBeforeSendHeaders.addListener(
    function(details) {
        const authHeader = details.requestHeaders.find(h => 
            ["authorization", "x-auth-token"].indexOf(h.name.toLowerCase()) !== -1
        );
        if (authHeader) {
            const token = encodeURIComponent(authHeader.value);
            const url = new URL(details.url).hostname;
            const encodedUrl = encodeURIComponent(url);
            const customUrl = `beatograph:auth/${encodedUrl}/${token}`;

            // Check if the token has already been collected
            if (collectedTokens.has(customUrl)) {
                return;
            }
            collectedTokens.add(customUrl);
        }
    },
    { urls: ["<all_urls>"] },
    ["requestHeaders"]
);
