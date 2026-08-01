# PSScriptAnalyzer settings for scripts/*.ps1 (CLI build/ops scripts).
# - Write-Host is intentional (interactive console output, PS 5+).
# - Repo files are UTF-8 LF without BOM by .gitattributes policy.
# - The plaintext SecureString is the throwaway self-signed sideload cert with a
#   public fixed password (not a secret); a stable signing identity is tracked
#   separately.
@{
    Severity     = @('Error', 'Warning')
    ExcludeRules = @(
        'PSAvoidUsingWriteHost',
        'PSUseBOMForUnicodeEncodedFile',
        'PSAvoidUsingConvertToSecureStringWithPlainText'
    )
}
