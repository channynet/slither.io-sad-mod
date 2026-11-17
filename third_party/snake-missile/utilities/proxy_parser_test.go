package utilities

import "testing"

func TestProxyAddresses(t *testing.T) {
	for _, input := range []string{"host:8080", "http://host:8080", "u:p@host:8080", "http://u:p@host:8080", "u:p@http://host:8080", "socks5://u:p@host:8080"} {
		got, err := ParseProxyURL(input)
		if err != nil || got.Hostname() != "host" || got.Port() != "8080" {
			t.Fatalf("failed %q: %v", input, err)
		}
	}
	for _, input := range []string{"http://", "https://user:secret@", "ftp://host:8080", "host:bad", "host:8080/path"} {
		if _, err := ParseProxyURL(input); err == nil {
			t.Fatalf("accepted invalid address %q", input)
		}
	}
	if got, err := ParseProxyURL("  "); err != nil || got != nil {
		t.Fatal("empty proxy should connect directly")
	}
}
