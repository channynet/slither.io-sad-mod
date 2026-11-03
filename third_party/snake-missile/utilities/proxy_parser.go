package utilities

import (
	"fmt"
	"net/url"
	"strings"
)

func ParseProxyURL(proxyString string) (*url.URL, error) {
	if proxyString == "" || len(strings.TrimSpace(proxyString)) == 0 {
		return nil, nil
	}

	address := strings.TrimSpace(proxyString)
	// Retain the upstream user:pass@scheme://host:port spelling as well.
	if at := strings.Index(address, "@"); at >= 0 && strings.Contains(address[at+1:], "://") {
		parts := strings.SplitN(address[at+1:], "://", 2)
		address = parts[0] + "://" + address[:at+1] + parts[1]
	}
	if !strings.Contains(address, "://") {
		address = "http://" + address
	}
	parsed, err := url.Parse(address)
	if err != nil || parsed.Hostname() == "" || parsed.Port() == "" ||
		(parsed.Scheme != "http" && parsed.Scheme != "https" && parsed.Scheme != "socks5" && parsed.Scheme != "socks5h") ||
		(parsed.Path != "" && parsed.Path != "/") || parsed.RawQuery != "" || parsed.Fragment != "" {
		return nil, fmt.Errorf("invalid proxy address")
	}
	return parsed, nil
}
