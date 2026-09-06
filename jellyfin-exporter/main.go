package main

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"strconv"
	"strings"
	"time"
)

const metricHeaders = `# HELP jellyfin_sessions_api_up Whether the Jellyfin sessions API is reachable.
# TYPE jellyfin_sessions_api_up gauge
# HELP jellyfin_sessions_playing Number of current playback sessions.
# TYPE jellyfin_sessions_playing gauge
# HELP jellyfin_sessions_users Number of users with current playback sessions.
# TYPE jellyfin_sessions_users gauge
# HELP jellyfin_sessions_transcoding Number of current transcoding sessions.
# TYPE jellyfin_sessions_transcoding gauge
# HELP jellyfin_session_info Current playback session information.
# TYPE jellyfin_session_info gauge
# HELP jellyfin_session_paused Whether the playback session is paused.
# TYPE jellyfin_session_paused gauge
# HELP jellyfin_session_position_seconds Current playback position.
# TYPE jellyfin_session_position_seconds gauge
# HELP jellyfin_session_duration_seconds Media duration.
# TYPE jellyfin_session_duration_seconds gauge
`

type item struct {
	Name         string
	SeriesName   string
	Type         string
	RunTimeTicks *int64
}

type playState struct {
	IsPaused      bool
	PlayMethod    string
	PositionTicks *int64
}

type transcode struct {
	HardwareAccelerationType string
}

type session struct {
	ID              string `json:"Id"`
	UserName        string
	Client          string
	DeviceName      string
	NowPlayingItem  *item
	PlayState       *playState
	TranscodingInfo *transcode
}

type exporter struct {
	client *http.Client
	url    string
	token  string
}

func escape(value string) string {
	return strings.NewReplacer("\\", "\\\\", "\"", "\\\"", "\r", "\\n", "\n", "\\n").Replace(value)
}

func labels(values ...string) string {
	names := []string{"session_id", "username", "client", "device", "title", "series", "media_type", "play_method", "hardware_acceleration"}
	parts := make([]string, len(values))
	for i := range values {
		parts[i] = names[i] + `="` + escape(values[i]) + `"`
	}
	return strings.Join(parts, ",")
}

func render(sessions []session) string {
	var body strings.Builder
	body.WriteString(metricHeaders)
	body.WriteString("jellyfin_sessions_api_up 1\n")
	users := map[string]struct{}{}
	playing := 0
	transcoding := 0
	for i, session := range sessions {
		if session.NowPlayingItem == nil {
			continue
		}
		playing++
		users[session.UserName] = struct{}{}
		id := session.ID
		if id == "" {
			id = strconv.Itoa(i)
		}
		method := ""
		paused := false
		if session.PlayState != nil {
			method = strings.ToLower(session.PlayState.PlayMethod)
			paused = session.PlayState.IsPaused
		}
		hardware := ""
		if session.TranscodingInfo != nil {
			hardware = session.TranscodingInfo.HardwareAccelerationType
		}
		if method == "transcode" || session.TranscodingInfo != nil {
			transcoding++
		}
		labelSet := labels(id, session.UserName, session.Client, session.DeviceName, session.NowPlayingItem.Name, session.NowPlayingItem.SeriesName, session.NowPlayingItem.Type, method, hardware)
		fmt.Fprintf(&body, "jellyfin_session_info{%s} 1\n", labelSet)
		pausedValue := 0
		if paused {
			pausedValue = 1
		}
		fmt.Fprintf(&body, "jellyfin_session_paused{session_id=\"%s\"} %d\n", escape(id), pausedValue)
		if session.PlayState != nil && session.PlayState.PositionTicks != nil {
			fmt.Fprintf(&body, "jellyfin_session_position_seconds{session_id=\"%s\"} %g\n", escape(id), float64(*session.PlayState.PositionTicks)/1e7)
		}
		if session.NowPlayingItem.RunTimeTicks != nil {
			fmt.Fprintf(&body, "jellyfin_session_duration_seconds{session_id=\"%s\"} %g\n", escape(id), float64(*session.NowPlayingItem.RunTimeTicks)/1e7)
		}
	}
	fmt.Fprintf(&body, "jellyfin_sessions_playing %d\n", playing)
	fmt.Fprintf(&body, "jellyfin_sessions_users %d\n", len(users))
	fmt.Fprintf(&body, "jellyfin_sessions_transcoding %d\n", transcoding)
	return body.String()
}

func (e exporter) collect(ctx context.Context) (string, error) {
	request, err := http.NewRequestWithContext(ctx, http.MethodGet, strings.TrimRight(e.url, "/")+"/Sessions", nil)
	if err != nil {
		return "", err
	}
	request.Header.Set("Accept", "application/json")
	request.Header.Set("X-Emby-Token", e.token)
	response, err := e.client.Do(request)
	if err != nil {
		return "", err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return "", fmt.Errorf("sessions API returned %s", response.Status)
	}
	var sessions []session
	if err := json.NewDecoder(io.LimitReader(response.Body, 8<<20)).Decode(&sessions); err != nil {
		return "", err
	}
	return render(sessions), nil
}

func main() {
	jellyfinURL := os.Getenv("JELLYFIN_URL")
	token := os.Getenv("JELLYFIN_TOKEN")
	parsed, err := url.Parse(jellyfinURL)
	if err != nil || (parsed.Scheme != "http" && parsed.Scheme != "https") || parsed.Host == "" || token == "" {
		log.Fatal("JELLYFIN_URL and JELLYFIN_TOKEN are required")
	}
	exporter := exporter{client: &http.Client{Timeout: 5 * time.Second}, url: jellyfinURL, token: token}
	http.HandleFunc("/metrics", func(writer http.ResponseWriter, request *http.Request) {
		writer.Header().Set("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
		metrics, err := exporter.collect(request.Context())
		if err != nil {
			log.Printf("sessions scrape failed: %v", err)
			fmt.Fprint(writer, metricHeaders+"jellyfin_sessions_api_up 0\n")
			return
		}
		fmt.Fprint(writer, metrics)
	})
	server := &http.Server{Addr: ":9594", ReadHeaderTimeout: 5 * time.Second}
	log.Fatal(server.ListenAndServe())
}
