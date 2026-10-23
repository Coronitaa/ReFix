using System;
using System.IO;
using System.Net.Security;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Cysharp.Threading.Tasks;
using Newtonsoft.Json.Linq;
using UnityEngine;

namespace ReFixAiBridge
{
    public static class Bridge
    {
        public static UniTask<JObject> SendAsync(
            JObject body,
            string key,
            int timeoutSeconds,
            CancellationToken cancellationToken,
            string endpoint,
            string provider)
        {
            return UniTask.RunOnThreadPool(() => ExecuteRequest(body, key, timeoutSeconds, cancellationToken, endpoint, provider), configureAwait: false, cancellationToken: cancellationToken);
        }

        private static JObject ExecuteRequest(
            JObject body,
            string key,
            int timeoutSeconds,
            CancellationToken cancellationToken,
            string endpoint,
            string provider)
        {
            if (timeoutSeconds <= 0) timeoutSeconds = 60;

            string targetUrl = endpoint;
            if (string.IsNullOrEmpty(targetUrl))
            {
                targetUrl = "https://openrouter.ai/api/v1/chat/completions";
            }

            try
            {
                return SendDirectHttps(targetUrl, key, body, timeoutSeconds, cancellationToken);
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[ReFixAiBridge] Primary request to {targetUrl} failed: {ex.Message}. Attempting resilient fallback...");

                // Fallback 1: If using OpenRouter and key is present, try a high-reliability free model on OpenRouter
                if (targetUrl.Contains("openrouter.ai") && !string.IsNullOrWhiteSpace(key))
                {
                    try
                    {
                        var orFallbackBody = (JObject)body.DeepClone();
                        orFallbackBody["model"] = "google/gemini-2.0-flash-lite-preview-02-05:free";
                        return SendDirectHttps(targetUrl, key, orFallbackBody, timeoutSeconds, cancellationToken);
                    }
                    catch (Exception orEx)
                    {
                        Debug.LogWarning($"[ReFixAiBridge] OpenRouter fallback failed: {orEx.Message}");
                    }
                }

                // Fallback 2: Pollinations
                if (!targetUrl.Contains("pollinations"))
                {
                    try
                    {
                        var fallbackBody = (JObject)body.DeepClone();
                        fallbackBody["model"] = "openai";
                        return SendDirectHttps("https://text.pollinations.ai/openai/chat/completions", "", fallbackBody, 30, cancellationToken);
                    }
                    catch (Exception fallbackEx)
                    {
                        Debug.LogError($"[ReFixAiBridge] Secondary fallback failed: {fallbackEx.Message}");
                    }
                }

                throw;
            }
        }

        private static JObject SendDirectHttps(
            string url,
            string apiKey,
            JObject body,
            int timeoutSeconds,
            CancellationToken cancellationToken)
        {
            var uri = new Uri(url);
            string host = uri.Host;
            int port = uri.Port > 0 ? uri.Port : 443;
            string pathAndQuery = uri.PathAndQuery;

            byte[] payloadBytes = Encoding.UTF8.GetBytes(body.ToString(Newtonsoft.Json.Formatting.None));
            Debug.Log($"[ReFixAiBridge] SslStream POST to https://{host}:{port}{pathAndQuery} ({payloadBytes.Length} bytes)");

            using (var tcp = new TcpClient())
            {
                tcp.SendTimeout = timeoutSeconds * 1000;
                tcp.ReceiveTimeout = timeoutSeconds * 1000;

                using (cancellationToken.Register(() => {
                    try { tcp.Close(); } catch {}
                }))
                {
                    tcp.Connect(host, port);

                    using (var ssl = new SslStream(tcp.GetStream(), false, (s, c, ch, err) => true))
                    {
                        ssl.AuthenticateAsClient(host);

                        var sb = new StringBuilder();
                        sb.Append($"POST {pathAndQuery} HTTP/1.1\r\n");
                        sb.Append($"Host: {host}\r\n");
                        if (!string.IsNullOrWhiteSpace(apiKey))
                            sb.Append($"Authorization: Bearer {apiKey.Trim()}\r\n");
                        sb.Append("Content-Type: application/json; charset=utf-8\r\n");
                        sb.Append($"Content-Length: {payloadBytes.Length}\r\n");
                        sb.Append("User-Agent: ReFix/1.0\r\n");
                        sb.Append("HTTP-Referer: https://github.com/Coronitaa/ReFix\r\n");
                        sb.Append("X-Title: ReFix Online\r\n");
                        sb.Append("Connection: close\r\n\r\n");

                        byte[] headerBytes = Encoding.UTF8.GetBytes(sb.ToString());
                        ssl.Write(headerBytes, 0, headerBytes.Length);
                        ssl.Write(payloadBytes, 0, payloadBytes.Length);
                        ssl.Flush();

                        byte[] rawBytes;
                        using (var ms = new MemoryStream())
                        {
                            byte[] buffer = new byte[8192];
                            int read;
                            while ((read = ssl.Read(buffer, 0, buffer.Length)) > 0)
                            {
                                ms.Write(buffer, 0, read);
                            }
                            rawBytes = ms.ToArray();
                        }

                        int headerEnd = FindHeaderEnd(rawBytes);
                        if (headerEnd < 0)
                            throw new Exception($"Invalid HTTP response: header delimiter not found (raw length {rawBytes.Length})");

                        string headerSection = Encoding.ASCII.GetString(rawBytes, 0, headerEnd);
                        byte[] bodyBytes = new byte[rawBytes.Length - (headerEnd + 4)];
                        Buffer.BlockCopy(rawBytes, headerEnd + 4, bodyBytes, 0, bodyBytes.Length);

                        int firstLineEnd = headerSection.IndexOf("\r\n");
                        string statusLine = firstLineEnd >= 0 ? headerSection.Substring(0, firstLineEnd) : headerSection;
                        Debug.Log($"[ReFixAiBridge] Server response: {statusLine}");

                        bool isChunked = headerSection.IndexOf("Transfer-Encoding: chunked", StringComparison.OrdinalIgnoreCase) >= 0;
                        byte[] decodedBytes = isChunked ? DecodeChunkedBytes(bodyBytes) : bodyBytes;
                        string jsonBody = Encoding.UTF8.GetString(decodedBytes).Trim();

                        if (!statusLine.Contains("200"))
                        {
                            throw new Exception($"Server returned {statusLine}: {jsonBody}");
                        }

                        JObject parsed;
                        try
                        {
                            parsed = JObject.Parse(jsonBody);
                        }
                        catch (Newtonsoft.Json.JsonReaderException)
                        {
                            int firstBrace = jsonBody.IndexOf('{');
                            int lastBrace = jsonBody.LastIndexOf('}');
                            if (firstBrace >= 0 && lastBrace > firstBrace)
                            {
                                string trimmedJson = jsonBody.Substring(firstBrace, (lastBrace - firstBrace) + 1);
                                parsed = JObject.Parse(trimmedJson);
                            }
                            else
                            {
                                throw;
                            }
                        }

                        if (parsed["error"] != null)
                        {
                            string errMsg = parsed["error"]?["message"]?.ToString() ?? "Unknown error from AI provider";
                            throw new Exception($"Provider returned error: {errMsg}");
                        }

                        // Normalize response content for CallerAiResponseContract
                        var choices = parsed["choices"] as JArray;
                        if (choices != null && choices.Count > 0)
                        {
                            var msg = choices[0]["message"] as JObject;
                            if (msg != null && msg["content"] != null)
                            {
                                string raw = msg["content"].ToString();
                                string norm = NormalizeModelOutput(raw);
                                msg["content"] = norm;
                                Debug.Log($"[ReFixAiBridge] Normalized reply for game: {norm}");
                            }
                        }

                        return parsed;
                    }
                }
            }
        }

        private static int FindHeaderEnd(byte[] data)
        {
            for (int i = 0; i < data.Length - 3; i++)
            {
                if (data[i] == (byte)'\r' && data[i + 1] == (byte)'\n' &&
                    data[i + 2] == (byte)'\r' && data[i + 3] == (byte)'\n')
                {
                    return i;
                }
            }
            return -1;
        }

        private static int FindCrlf(byte[] data, int start)
        {
            for (int i = start; i < data.Length - 1; i++)
            {
                if (data[i] == (byte)'\r' && data[i + 1] == (byte)'\n')
                {
                    return i;
                }
            }
            return -1;
        }

        private static byte[] DecodeChunkedBytes(byte[] data)
        {
            using (var ms = new MemoryStream())
            {
                int pos = 0;
                while (pos < data.Length)
                {
                    int crlf = FindCrlf(data, pos);
                    if (crlf < 0) break;

                    string hexLine = Encoding.ASCII.GetString(data, pos, crlf - pos).Trim();
                    if (string.IsNullOrEmpty(hexLine))
                    {
                        pos = crlf + 2;
                        continue;
                    }

                    int semi = hexLine.IndexOf(';');
                    if (semi >= 0) hexLine = hexLine.Substring(0, semi).Trim();

                    int chunkSize;
                    try
                    {
                        chunkSize = Convert.ToInt32(hexLine, 16);
                    }
                    catch
                    {
                        break;
                    }

                    if (chunkSize == 0) break;

                    int chunkDataStart = crlf + 2;
                    int available = data.Length - chunkDataStart;
                    int toCopy = Math.Min(chunkSize, available);
                    if (toCopy > 0)
                    {
                        ms.Write(data, chunkDataStart, toCopy);
                    }

                    pos = chunkDataStart + chunkSize + 2; // skip chunk data + trailing \r\n
                }
                return ms.ToArray();
            }
        }

        private static string NormalizeModelOutput(string content)
        {
            if (string.IsNullOrWhiteSpace(content))
            {
                return "{\"dialogue\":\"¿Hola?\",\"trust_percent\":50,\"emotion\":\"NEUTRAL\"}";
            }

            string s = content.Trim();

            // Strip markdown block ```json ... ```
            if (s.StartsWith("```"))
            {
                int firstNewline = s.IndexOf('\n');
                if (firstNewline >= 0) s = s.Substring(firstNewline + 1);
                if (s.EndsWith("```")) s = s.Substring(0, s.Length - 3).Trim();
            }

            s = s.Trim();

            // If already valid JSON object with dialogue
            if (s.StartsWith("{") && s.EndsWith("}"))
            {
                try
                {
                    var test = JObject.Parse(s);
                    if (test["dialogue"] != null) return s;
                }
                catch {}
            }

            // If it's a JSON array: [ "dialogue", { "trust_percent": 50, ... } ]
            if (s.StartsWith("[") && s.EndsWith("]"))
            {
                try
                {
                    var arr = JArray.Parse(s);
                    string dialogue = "";
                    int trust = 50;
                    string emotion = "NEUTRAL";

                    foreach (var item in arr)
                    {
                        if (item.Type == JTokenType.String && string.IsNullOrEmpty(dialogue))
                        {
                            dialogue = item.ToString();
                        }
                        else if (item is JObject objItem)
                        {
                            if (objItem["dialogue"] != null && string.IsNullOrEmpty(dialogue))
                                dialogue = objItem["dialogue"].ToString();
                            if (objItem["trust_percent"] != null && int.TryParse(objItem["trust_percent"].ToString(), out int t))
                                trust = t;
                            if (objItem["emotion"] != null)
                                emotion = objItem["emotion"].ToString();
                        }
                    }

                    if (!string.IsNullOrWhiteSpace(dialogue))
                    {
                        var result = new JObject
                        {
                            ["dialogue"] = dialogue,
                            ["trust_percent"] = trust,
                            ["emotion"] = emotion,
                            ["completed_objectives"] = new JArray()
                        };
                        return result.ToString(Newtonsoft.Json.Formatting.None);
                    }
                }
                catch {}
            }

            // Plain text dialogue fallback
            var plainObj = new JObject
            {
                ["dialogue"] = s,
                ["trust_percent"] = 50,
                ["emotion"] = "NEUTRAL",
                ["completed_objectives"] = new JArray()
            };
            return plainObj.ToString(Newtonsoft.Json.Formatting.None);
        }
    }
}
