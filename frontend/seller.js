function addProduct() {

    let name = document.getElementById("productName").value;
    let price = document.getElementById("productPrice").value;
    let quantity = document.getElementById("productQuantity").value;
    let description = document.getElementById("productDescription").value;

    if (name === "" || price === "" || quantity === "" || description === "") {
        alert("Please fill all product details.");
        return;
    }

    let product = {
        name: name,
        price: price,
        quantity: quantity,
        description: description
    };

    let products = JSON.parse(localStorage.getItem("sellerProducts")) || [];

    products.push(product);

    localStorage.setItem("sellerProducts", JSON.stringify(products));

    alert("Product added successfully!");

    document.getElementById("productName").value = "";
    document.getElementById("productPrice").value = "";
    document.getElementById("productQuantity").value = "";
    document.getElementById("productDescription").value = "";

    displayProducts();
}


function displayProducts() {

    let products = JSON.parse(localStorage.getItem("sellerProducts")) || [];

    let container = document.getElementById("sellerProducts");

    container.innerHTML = "";

    products.forEach(function(product, index) {

        container.innerHTML += `
            <div class="product">
                <h3>${product.name}</h3>
                <p>Price: ₹${product.price}</p>
                <p>Quantity: ${product.quantity}</p>
                <p>${product.description}</p>

                <button onclick="deleteProduct(${index})">
                    Delete Product
                </button>
            </div>
            <br>
        `;
    });
}


function deleteProduct(index) {

    let products = JSON.parse(localStorage.getItem("sellerProducts")) || [];

    products.splice(index, 1);

    localStorage.setItem("sellerProducts", JSON.stringify(products));

    displayProducts();
}


displayProducts();