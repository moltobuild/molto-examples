# Exercise the live API with Windows PowerShell 5.1 or PowerShell 7+.
param([string]$BaseUrl = 'http://127.0.0.1:8080')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Net.Http
$client = New-Object System.Net.Http.HttpClient
$client.Timeout = [TimeSpan]::FromSeconds(5)
$BaseUrl = $BaseUrl.TrimEnd('/')
$itemId = $null

function Request([string]$Method, [string]$Path, [int]$Expected,
                 [string]$Body = $null, [string]$ContentType = 'application/json') {
    $message = New-Object System.Net.Http.HttpRequestMessage
    $message.Method = New-Object System.Net.Http.HttpMethod($Method)
    $message.RequestUri = [Uri]($BaseUrl + $Path)
    if ($null -ne $Body -and $Body.Length -gt 0) {
        $message.Content = New-Object System.Net.Http.StringContent($Body, [Text.Encoding]::UTF8, $ContentType)
    }
    $reply = $null
    try {
        $reply = $client.SendAsync($message).GetAwaiter().GetResult()
        $text = $reply.Content.ReadAsStringAsync().GetAwaiter().GetResult()
        if ([int]$reply.StatusCode -ne $Expected) {
            throw "$Method ${Path}: expected $Expected, got $([int]$reply.StatusCode): $text"
        }
        # Preserve JSON arrays, including empty arrays, without pipeline unrolling.
        return @{ Text = $text; Json = $(if ($text.Length -gt 0) { ConvertFrom-Json -InputObject $text }) }
    } finally {
        if ($null -ne $reply) { $reply.Dispose() }
        $message.Dispose()
    }
}
function Check([bool]$Condition, [string]$Label) {
    if (-not $Condition) { throw "Assertion failed: $Label" }
}
try {
    $null = Request GET /health 200
    $name = "O'Reilly `"keyboard`" Caf$([char]0xE9); DROP TABLE items; --"
    $body = @{ name = $name; price = 19.99; stock = 4 } | ConvertTo-Json -Compress
    $item = (Request POST /items 201 $body).Json
    $itemId = $item.id
    Check ($itemId -gt 0) 'positive item ID'
    $path = "/items/$itemId"
    Check ($item.name -ceq $name -and $item.price -eq 19.99 -and $item.stock -eq 4) 'created fields'
    $created = $item.created_at
    $updated = $item.updated_at
    $fetched = (Request GET $path 200).Json
    Check (($fetched | ConvertTo-Json -Compress) -ceq ($item | ConvertTo-Json -Compress)) 'get item'
    $page = Request GET '/items?limit=2&offset=0' 200
    Check ($page.Text.TrimStart().StartsWith('[') -and @($page.Json).Count -le 2) 'pagination'
    $patched = (Request PATCH $path 200 '{"stock":0}').Json
    Check ($patched.name -ceq $name -and $patched.price -eq 19.99 -and $patched.stock -eq 0) 'partial update'
    Check ($patched.created_at -eq $created) 'created timestamp preserved'
    Check ([DateTimeOffset]$patched.updated_at -gt [DateTimeOffset]$updated) 'updated timestamp advances'
    $renamed = (Request PATCH $path 200 '{"name":"Mouse","price":7.50}').Json
    Check ($renamed.name -ceq 'Mouse' -and $renamed.price -eq 7.5 -and $renamed.stock -eq 0) 'rename and price update'
    $null = Request PATCH $path 422 '{"created_at":"now"}'
    $null = Request PATCH $path 422 '{}'
    $null = Request POST /items 422 '{"name":"Bad","price":-1,"stock":0}'
    $null = Request POST /items 400 '[]'
    $null = Request POST /items 415 '{}' 'text/plain'
    $null = Request POST /items 413 (' ' * 4097)
    $null = Request GET '/items?limit=0' 400
    $null = Request GET /items/0 400
    $null = Request GET /missing 404
    $null = Request PUT $path 405
    $deleted = Request DELETE $path 204
    Check ($deleted.Text.Length -eq 0) 'empty delete response'
    $null = Request GET $path 404
    $null = Request DELETE $path 404
    $itemId = $null
    Write-Output 'smoke: all CRUD and HTTP checks passed'
} finally {
    if ($null -ne $itemId) {
        try { $null = Request DELETE "/items/$itemId" 204 }
        catch { Write-Warning "Cleanup failed: $_" }
    }
    $client.Dispose()
}
