const API_BASE_URL = "http://localhost:8080/api";

// API helper
async function apiRequest(endpoint, method = "GET", data = null) {
    try {
        const options = {
            method: method,
            headers: {
                "Content-Type": "application/json"
            }
        };

        if (data) {
            options.body = JSON.stringify(data);
        }

        const response = await fetch(API_BASE_URL + endpoint, options);

        if (!response.ok) {
            throw new Error("API request failed");
        }

        return await response.json();

    } catch (error) {
        console.log("API not available:", error.message);
        return null;
    }
}

// Add product to cart
function addToCart(name, price) {

    let cart = JSON.parse(localStorage.getItem("yaluMartCart")) || [];

    cart.push({
        name: name,
        price: Number(price),
        quantity: 1
    });

    localStorage.setItem("yaluMartCart", JSON.stringify(cart));

    alert(name + " added to cart!");
}

// Get products from API
async function loadProductsFromAPI() {

    const products = await apiRequest("/products");

    if (products) {
        console.log("Products received from YaluMart API:", products);
    }
}