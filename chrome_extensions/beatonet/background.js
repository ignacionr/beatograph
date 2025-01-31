chrome.webRequest.onBeforeSendHeaders.addListener(
    function(details) {
        const authHeader = details.requestHeaders.find(h => h.name.toLowerCase() === "authorization");
        if (authHeader) {
            const token = encodeURIComponent(authHeader.value);
            const url = encodeURIComponent(details.url);
            const customUrl = `beatograph:auth/${url}/${token}`;

            console.log("Launching:", customUrl);

            // Open the custom URL
            chrome.tabs.update({ url: customUrl });
        }
    },
    { urls: ["<all_urls>"] },
    ["requestHeaders"]
);
