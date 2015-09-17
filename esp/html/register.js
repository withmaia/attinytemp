function $(s) { return document.querySelector(s); };
var status_text = $('.status');
document.forms['register'].style.display = 'none';

// Check registration status
var check_registration_interval = null;

function checkRegistration() {
    var fetch_registration = fetch('/registration.json').then(function(resp) {
        return resp.json();
    });
    fetch_registration.then(handleRegistration);
};

// Interpret and react to registration status
function handleRegistration(registration) {
    if (registration.status == 'registered') {
        clearInterval(check_registration_interval);
        document.forms['register'].style.display = 'none';
        status_text.innerHTML =
            "<p class='help'>Your device is now registered to " + registration.email + "</p>"
            + "<p class='note'>Remember to switch back to your home wifi network!</p>";
    }

    else if (registration.status == 'failed') {
        document.forms['register'].style.display = 'block';
        $('form[name=register] button').textContent = 'Try again';
        $('form[name=register] button').disabled = false;
        if (registration.error) {
            status_text.innerHTML = '<strong>Failed to register</strong>: ' + registration.error
        } else {
            status_text.innerHTML = '<strong>Failed to register</strong>'
        }
        status_text.className = 'status error'
    }

    else {
        document.forms['register'].style.display = 'block';
        $('#token').focus();
        status_text.textContent = '';
    }
}

checkRegistration(); // Start by checking registration

// Submit registration form -> post registration, start checking registration status
document.forms['register'].onsubmit = function() {
    $('form[name=register] button').textContent = 'Registering...';
    $('form[name=register] button').disabled = true;
    status_text.textContent = '';
    status_text.className = 'status';

    fetch('/register.json', {
        method: 'post',
        body: JSON.stringify({
            token: $('#token').value
        })
    });

    check_registration_interval = setInterval(checkRegistration, 2500);
    return false;
};

