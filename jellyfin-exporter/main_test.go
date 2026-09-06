package main

import (
	"bytes"
	"context"
	"io"
	"net/http"
	"strings"
	"testing"
	"time"
)

type roundTripFunc func(*http.Request) (*http.Response, error)

func (function roundTripFunc) RoundTrip(request *http.Request) (*http.Response, error) {
	return function(request)
}

func TestCollect(t *testing.T) {
	client := &http.Client{Timeout: time.Second, Transport: roundTripFunc(func(request *http.Request) (*http.Response, error) {
		if request.Header.Get("X-Emby-Token") != "secret" {
			t.Fatal("missing token")
		}
		body := bytes.NewBufferString(`[{"Id":"one","UserName":"alice","Client":"TV","DeviceName":"Living \"Room\"","NowPlayingItem":{"Name":"Movie\nNight","Type":"Movie","RunTimeTicks":20000000},"PlayState":{"PlayMethod":"DirectPlay","PositionTicks":10000000}},{"Id":"two","UserName":"alice","NowPlayingItem":{"Name":"Episode","Type":"Episode"},"PlayState":{"PlayMethod":"Transcode","IsPaused":true},"TranscodingInfo":{"HardwareAccelerationType":"qsv"}}]`)
		return &http.Response{StatusCode: http.StatusOK, Body: io.NopCloser(body)}, nil
	})}
	exporter := exporter{client: client, url: "http://jellyfin", token: "secret"}
	metrics, err := exporter.collect(context.Background())
	if err != nil {
		t.Fatal(err)
	}
	for _, expected := range []string{"jellyfin_sessions_playing 2", "jellyfin_sessions_users 1", "jellyfin_sessions_transcoding 1", `device="Living \"Room\""`, `title="Movie\nNight"`, "jellyfin_session_position_seconds{session_id=\"one\"} 1"} {
		if !strings.Contains(metrics, expected) {
			t.Errorf("missing %q in:\n%s", expected, metrics)
		}
	}
}
